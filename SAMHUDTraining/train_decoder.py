"""Finetune the MobileSAM mask decoder on SAMHUDTraining pairs.

The image encoder stays frozen. A loose box around each cutout is the prompt,
matching Magic Select. Exports a drop-in decoder ONNX.
"""

import random
import sys
from pathlib import Path

import numpy as np
import onnxruntime as ort
import torch
import torch.nn.functional as F
from PIL import Image

from mobile_sam import sam_model_registry
from mobile_sam.utils.onnx import SamOnnxModel
from mobile_sam.utils.transforms import ResizeLongestSide

ROOT = Path(__file__).resolve().parent
CKPT = ROOT / "weights" / "mobile_sam.pt"
ENCODER = ROOT.parent / "data" / "models" / "mobilesam.encoder.onnx"
DECODER_OUT = ROOT.parent / "data" / "models" / "mobilesam.decoder.onnx"
DECODER_BASE = ROOT.parent / "data" / "models" / "mobilesam.decoder.base.onnx"


def pairs():
	found = []
	for still in ROOT.rglob("*.png"):
		if still.name.count(".") != 1:
			continue
		for mask in still.parent.glob(still.stem + ".*.png"):
			found.append((still, mask))
	return found


def load_pair(still_path, mask_path):
	still = np.array(Image.open(still_path).convert("RGB"))
	mask = np.array(Image.open(mask_path).convert("L"))
	mask = (mask >= 128).astype(np.float32)
	return still, mask


def bbox(mask):
	ys, xs = np.where(mask >= 0.5)
	if len(xs) == 0:
		return None
	return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def crop_example(image, mask, rng, loose=None):
	box = bbox(mask)
	if box is None:
		return None
	x0, y0, x1, y1 = box
	h, w = mask.shape
	if loose is None:
		left = rng.randint(12, 72)
		top = rng.randint(12, 72)
		right = rng.randint(12, 72)
		bottom = rng.randint(12, 72)
	else:
		left = top = right = bottom = loose
	cx0 = max(0, x0 - left)
	cy0 = max(0, y0 - top)
	cx1 = min(w - 1, x1 + right)
	cy1 = min(h - 1, y1 + bottom)
	if cx1 - cx0 < 8 or cy1 - cy0 < 8:
		return None
	crop_img = image[cy0 : cy1 + 1, cx0 : cx1 + 1]
	crop_mask = mask[cy0 : cy1 + 1, cx0 : cx1 + 1]
	bx0 = float(np.clip(x0 - cx0 + rng.randint(-6, 8), 0, crop_img.shape[1] - 1))
	by0 = float(np.clip(y0 - cy0 + rng.randint(-6, 8), 0, crop_img.shape[0] - 1))
	bx1 = float(np.clip(x1 - cx0 + rng.randint(-6, 8), 0, crop_img.shape[1] - 1))
	by1 = float(np.clip(y1 - cy0 + rng.randint(-6, 8), 0, crop_img.shape[0] - 1))
	if bx1 < bx0:
		bx0, bx1 = bx1, bx0
	if by1 < by0:
		by0, by1 = by1, by0
	return crop_img, crop_mask, np.array([bx0, by0, bx1, by1], np.float32)


def dice_bce(logits, target):
	prob = torch.sigmoid(logits)
	bce = F.binary_cross_entropy_with_logits(logits, target)
	dims = (-1, -2)
	inter = (prob * target).sum(dims)
	den = prob.sum(dims) + target.sum(dims)
	dice = (1 - (2 * inter + 1) / (den + 1)).mean()
	return bce + dice


def batch_iou(logits, target):
	pred = (torch.sigmoid(logits) > 0.5).float()
	dims = (-1, -2)
	inter = (pred * target).sum(dims)
	union = pred.sum(dims) + target.sum(dims) - inter
	return (inter + 1) / (union + 1)


def forward_crop(sam, transform, image, mask, box, device):
	resized = transform.apply_image(image)
	tensor = torch.as_tensor(resized, device=device).permute(2, 0, 1).contiguous().float()
	with torch.no_grad():
		emb = sam.image_encoder(sam.preprocess(tensor[None]))
	model_box = transform.apply_boxes(box[None, :], image.shape[:2])
	box_t = torch.as_tensor(model_box, device=device, dtype=torch.float32)
	sparse, dense = sam.prompt_encoder(points=None, boxes=box_t, masks=None)
	low, iou_pred = sam.mask_decoder(
		image_embeddings=emb,
		image_pe=sam.prompt_encoder.get_dense_pe(),
		sparse_prompt_embeddings=sparse,
		dense_prompt_embeddings=dense,
		multimask_output=True,
	)
	up = sam.postprocess_masks(low, tensor.shape[-2:], image.shape[:2])
	target = torch.as_tensor(mask, device=device)[None, None].expand_as(up)
	per = []
	ious = []
	for c in range(up.shape[1]):
		per.append(dice_bce(up[:, c], target[:, 0]))
		ious.append(batch_iou(up[:, c], target[:, 0]))
	loss = torch.stack(per).min()
	iou_t = torch.stack(ious, dim=1).detach()
	loss = loss + F.mse_loss(iou_pred, iou_t)
	best = int(torch.stack([v.detach().reshape(-1)[0] for v in ious]).argmax())
	return loss, float(ious[best].detach())


def eval_set(sam, transform, items, device):
	scores = []
	rng = random.Random(0)
	for image, mask in items:
		ex = crop_example(image, mask, rng, loose=32)
		if ex is None:
			continue
		crop, gt, box = ex
		with torch.no_grad():
			_, score = forward_crop(sam, transform, crop, gt, box, device)
		scores.append(score)
	return float(np.mean(scores)) if scores else 0.0


def export_decoder(sam, path):
	sam = sam.cpu().eval()
	onnx_model = SamOnnxModel(model=sam, return_single_mask=True)
	embed_size = sam.prompt_encoder.image_embedding_size
	dummy = (
		torch.randn(1, 256, *embed_size),
		torch.randint(0, 1024, (1, 2, 2)).float(),
		torch.tensor([[2.0, 3.0]]),
		torch.zeros(1, 1, 256, 256),
		torch.zeros(1),
		torch.tensor([256.0, 256.0]),
	)
	path.parent.mkdir(parents=True, exist_ok=True)
	torch.onnx.export(
		onnx_model,
		dummy,
		str(path),
		input_names=["image_embeddings", "point_coords", "point_labels", "mask_input", "has_mask_input", "orig_im_size"],
		output_names=["masks", "iou_predictions", "low_res_masks"],
		opset_version=16,
		dynamo=False,
	)


def onnx_iou(encoder, decoder, image, mask):
	box = bbox(mask)
	if box is None:
		return None
	x0, y0, x1, y1 = box
	h, w = mask.shape
	cx0, cy0 = max(0, x0 - 32), max(0, y0 - 32)
	cx1, cy1 = min(w - 1, x1 + 32), min(h - 1, y1 + 32)
	crop = image[cy0 : cy1 + 1, cx0 : cx1 + 1]
	gt = mask[cy0 : cy1 + 1, cx0 : cx1 + 1]
	ch, cw = crop.shape[:2]
	scale = 1024.0 / max(ch, cw)
	nw = max(1, int(np.floor(cw * scale + 0.5)))
	nh = max(1, int(np.floor(ch * scale + 0.5)))
	resized = np.array(Image.fromarray(crop).resize((nw, nh), Image.BILINEAR)).astype(np.float32)
	emb = encoder.run(None, {"input_image": resized})[0]
	coords = np.array(
		[[[ (x0 - cx0) * scale, (y0 - cy0) * scale], [(x1 - cx0) * scale, (y1 - cy0) * scale]]],
		np.float32,
	)
	outs = decoder.run(
		None,
		{
			"image_embeddings": emb,
			"point_coords": coords,
			"point_labels": np.array([[2, 3]], np.float32),
			"mask_input": np.zeros((1, 1, 256, 256), np.float32),
			"has_mask_input": np.zeros((1,), np.float32),
			"orig_im_size": np.array([ch, cw], np.float32),
		},
	)
	pred = outs[0][0, 0] > 0
	inter = np.logical_and(pred, gt > 0.5).sum()
	union = np.logical_or(pred, gt > 0.5).sum()
	return float(inter / max(1, union))


def main():
	device = "cuda" if torch.cuda.is_available() else "cpu"
	items = []
	for still, mask in pairs():
		image, gt = load_pair(still, mask)
		if bbox(gt) is None:
			continue
		items.append((image, gt, f"{still.parent.name}/{mask.name}"))
	print(f"{len(items)} pairs on {device}")
	if not items:
		sys.exit(1)

	sam = sam_model_registry["vit_t"](checkpoint=str(CKPT))
	sam.to(device)
	for p in sam.image_encoder.parameters():
		p.requires_grad = False
	sam.image_encoder.eval()
	sam.prompt_encoder.train()
	sam.mask_decoder.train()
	transform = ResizeLongestSide(sam.image_encoder.img_size)
	opt = torch.optim.AdamW(
		list(sam.prompt_encoder.parameters()) + list(sam.mask_decoder.parameters()),
		lr=1e-4,
		weight_decay=1e-4,
	)

	base = eval_set(sam, transform, [(i, m) for i, m, _ in items], device)
	print(f"before {base:.3f}")

	rng = random.Random(1)
	for epoch in range(1, 41):
		rng.shuffle(items)
		losses = []
		for image, mask, _name in items:
			for _ in range(4):
				ex = crop_example(image, mask, rng)
				if ex is None:
					continue
				crop, gt, box = ex
				opt.zero_grad()
				loss, _score = forward_crop(sam, transform, crop, gt, box, device)
				loss.backward()
				opt.step()
				losses.append(float(loss))
		if epoch % 5 == 0 or epoch == 1:
			score = eval_set(sam, transform, [(i, m) for i, m, _ in items], device)
			print(f"epoch {epoch} loss {np.mean(losses):.3f} iou {score:.3f}")

	sam.eval()
	CKPT.parent.mkdir(parents=True, exist_ok=True)
	torch.save(sam.state_dict(), CKPT.parent / "decoder_hud.pt")
	if not DECODER_BASE.exists() and DECODER_OUT.exists():
		DECODER_BASE.write_bytes(DECODER_OUT.read_bytes())
	export_decoder(sam, DECODER_OUT)
	print(f"wrote {DECODER_OUT}")

	enc = ort.InferenceSession(str(ENCODER), providers=["CPUExecutionProvider"])
	old = ort.InferenceSession(str(DECODER_BASE), providers=["CPUExecutionProvider"])
	new = ort.InferenceSession(str(DECODER_OUT), providers=["CPUExecutionProvider"])
	old_scores, new_scores = [], []
	for image, mask, name in items:
		a = onnx_iou(enc, old, image, mask)
		b = onnx_iou(enc, new, image, mask)
		old_scores.append(a)
		new_scores.append(b)
		print(f"{name}  base {a:.3f}  tuned {b:.3f}")
	print(f"onnx mean  base {np.mean(old_scores):.3f}  tuned {np.mean(new_scores):.3f}")


if __name__ == "__main__":
	main()
