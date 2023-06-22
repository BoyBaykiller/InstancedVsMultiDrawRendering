# InstancedVsMultiDrawRendering

This is a small educational project investigating Instancing and Multi Draw.
The application renders a particular test scene using both mechanisms and prints timings and data about how the vertex shader is executed on the subgroup level.

## 1.0 Test procedure

Many single triangle meshes are rendered to the screen in two different ways:
1. As a single draw with multiple instances (Instanced Rendering)
2. As multiple draws with a single instance (Multi Draw Rendering)

The triangle's position is calculated using either `gl_InstanceID` or `gl_DrawID` respectivly. 
Vertices are hardcoded into a local array and indexed with `gl_VertexID`. Overall the shaders are very simple. The actual draw call used is `glMultiDrawArraysIndirect` which is capable of both instancing and multi drawing.

Here is the output with a triangle count of 10'000:
![10KTriangles](Screenshots/10kTriangles.bmp?raw=true)


## 2.0 Data evaluation

This is the reported data on a RTX 3050 Ti Laptop:

![Data](Screenshots/Data.bmp?raw=true)

We immediately notice that instanced rendering performs better. But why is that?

On Nvidia a subgroup is 32 invocations big. A single one can therefore process 32 vertices in parallel.
In our case one mesh has only 3 vertices, so as an optimization when instancing it packs vertices from multiple instances into the same subgroup.
"SubgroupUtilization" in the screenshot above shows that 30 of the 32 invocations are active, which tells us that it is indeed packing 10 instanced triangles into a single subgroup, thus reducing the total number of subgroups dispatched. 

So why isnt it applying the same optimization when multi drawing?

The OpenGL 4.6 specification contains the following statement about `gl_DrawID`:
> `gl_DrawID` holds the integer draw number the current draw being processed by the shader invocation. **It is dynamically uniform**.

In practice "dynamically uniform" means that the value of `gl_DrawID` must be the same for all active invocations inside a subgroup.
This requirement prevents the driver from doing the "subgroup-packing" optimization mentioned above.
Looking again at "SubgroupUtilization", this is confirmed by it only showing 3 out of 32 being active.
If it were to pack vertex shader invocations from different draws into the same subgroup then the invocations would not agree on the value of `gl_DrawID` which is against the spec.

---

## Note

The performance difference shown here is only so big because the mesh is tiny - literally a single triangle. With larger meshes the "subgroup-packing" optimization can be insignificant.

Some of the reported data on Intel seems wrong. Part of it might have to do with `gl_SubroupSize` being 32 while it actually appears to be running in wave8 mode. Basically this app only works correctly on AMD and Nvidia.

