# Wolf-Engine 2.0 - Android Sponza Demo

An Android demo showing the iconic Sponza atrium using [Wolf-Engine 2.0](https://github.com/arthur-monteiro/WolfEngine-2.0). 

The scene is composed of hundreds of individual object instances, fully baked and structured using the [Wolf-Engine 2.0 3D Editor](https://github.com/arthur-monteiro/Wolf-Engine-2.0--3D-Editor).

![Sponza Android Demo](screenshots/screenshot.gif)

## GPU-Driven Rendering Pipeline

* **Compute culling and LOD selection:** A dedicated compute shader pass evaluates every instance in parallel, performing frustum culling and dynamically selecting the appropriate Level of Detail (LOD).
* **Indirect drawing:** Culled instance data is written directly to GPU buffers and rendered via a forward pass using one `vkCmdDrawIndexedIndirectCount`.