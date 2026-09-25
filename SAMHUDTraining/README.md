# SAMHUDTraining

One still, one mask per element. Subfolders are labels for you. Pairing uses the filename only.

## Layout

```
SAMHUDTraining/
  Apex Legends/
    apex-01.png
    apex-01.gun.png
    apex-01.player.png
  Baldur's Gate 3/
    bg3-01.png
    bg3-01.hotbar.png
```

## Names

- Still: `{id}.png`
  Example: `apex-01.png`
  No extra dot. No pen marks, boxes, or circles.
- Mask: `{id}.{element}.png`
  Example: `apex-01.gun.png`
  `{element}` is one word: letters, numbers, or hyphens. No spaces.

The same still is reused for every element on that frame. Do not put every element in one mask.

## Mask

- Same width and height as its still, pixel for pixel.
- White is that element only, cut to the chrome you want kept.
- Black is everything else.
- Not a box. Not a loose circle. A filled rectangle teaches the model to return rectangles.

## Still

- The resolution OBS captures. Do not resize.
- Clean game frame only.

## Enough to start

About 30 masks total, across a few frames and games. A few weapons, a few bars, empty and filled slots. More can be added later in the same folders.
