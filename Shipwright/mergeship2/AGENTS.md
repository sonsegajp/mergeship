# MergeShip2 implementation constraints

- Do not use code from the original Mergeship project, including its soh_fd tree.
  The user explicitly rejected that implementation on 2026-09-22.
- Use the native MM implementation in 2ship2harkinian and the OoT implementation
  in Shipwright as the source references for this redo.
- Required result: assets usable in both runtimes, and all four MM transformation
  masks and their native abilities functional in OoT.
- Builds, asset parsing, and skeleton swaps are not gameplay completion evidence.
