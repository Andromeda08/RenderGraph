# RenderGraph

Prototype for a RenderGraph system and compiler with support for:
- Pass and resource culling.
- Task execution order generation.
- Async task scheduling.
- Memory optimization via aliasing.
- Automatic barrier and synchronization generation.

### Example RenderGraph
```mermaid
flowchart LR
    classDef resImage color:#4c4f69,fill:#cba6f7,stroke:#8839ef,stroke-width:1px;
    classDef resOther color:#4c4f69,fill:#f38ba8,stroke:#d20f39,stroke-width:1px;
    classDef pass color:#4c4f69,fill:#b4befe,stroke:#7287fd,stroke-width:1px;
    0[Root]:::pass
    1scene(scene):::resOther
    2[G-Buffer Pass]:::pass
    4positionImage(positionImage):::resImage
    5normalImage(normalImage):::resImage
    6albedoImage(albedoImage):::resImage
    4positionImage(positionImage):::resImage
    5normalImage(normalImage):::resImage
    8[Lighting Pass]:::pass
    12lightingResult(lightingResult):::resImage
    13[Ambient Occlusion Pass]:::pass
    16ambientOcclusionImage(ambientOcclusionImage):::resImage
    17[Composition Pass]:::pass
    20combined(combined):::resImage
    21[Present]:::pass
    0 --> 1scene
    1scene --> 2
    2 --> 4positionImage
    4positionImage --> 8
    2 --> 5normalImage
    5normalImage --> 8
    2 --> 6albedoImage
    6albedoImage --> 8
    4positionImage --> 13
    5normalImage --> 13
    8 --> 12lightingResult
    12lightingResult --> 17
    13 --> 16ambientOcclusionImage
    16ambientOcclusionImage --> 17
    17 --> 20combined
    20combined --> 21

```

### Compiler Output : Timeline
- Main rendering queue
- Async compute queue
- Resource allocations (labels represent usage for a range)
```mermaid
---
displayMode: compact
---
gantt
	dateFormat X
	axisFormat %s
	section Passes
		Root : 0, 1
		G-Buffer Pass : 1, 2
		Lighting Pass : 2, 3
		Composition Pass : 3, 4
		Present Pass : 4, 5
	section Async
		Ambient Occlusion Pass :crit, 2, 3
	section Resource #0
		combined : 3, 5
		positionImage : 1, 3
	section Resource #1
		normalImage : 1, 3
	section Resource #2
		albedoImage : 1, 3
	section Resource #3
		motionVectors : 1, 4
	section Resource #4
		ambientOcclusionImage : 2, 4
```