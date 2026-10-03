# Bunny test mesh

The simplify-bunny.cfg scene renders the Stanford bunny. The mesh is
the zippered reconstruction of the range scans (bun_zipper.ply), an
ASCII PLY with 35,947 vertices and 69,451 position-only faces; the
confidence and intensity properties of the file are skipped by
LuxCore's PLY reader.

The mesh is included here as bunny.ply (3 MB, renamed from
bun_zipper.ply). To regenerate it from the source:

    wget http://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz
    tar xzf bunny.tar.gz
    mv bunny/reconstruction/bun_zipper.ply ./bunny.ply

Source: the Stanford 3D Scanning Repository
(https://graphics.stanford.edu/data/3Dscanrep/), bunny.tar.gz (keep
renderings of the model in good taste, per the repository terms).

Reference numbers for a quick sanity check (simplify target 0.1): the
scene simplifies 69,451 down to 6,661 faces over 21 iterations, in a
fraction of a second on 20 threads. The rendered image is written to
image.png (the image.filename property of the configuration file is
ignored by the film output parsing). The wireframe border of this
scene is thinner than the Lucy and dragon ones (0.0005 instead of
0.002): the bunny is a few times smaller and its triangles bigger in
world units, so the wider border would cover most of the surface.
