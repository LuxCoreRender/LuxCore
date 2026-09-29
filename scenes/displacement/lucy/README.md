# Lucy test mesh

The simplify-lucy.cfg scene renders the Stanford Lucy statue. The mesh
is not included in this repository (533 MB): download it and place it
here as lucy.ply before rendering.

    wget https://graphics.stanford.edu/data/3Dscanrep/lucy.tar.gz
    tar xzf lucy.tar.gz
    mv lucy.ply ./

Source: the Stanford 3D Scanning Repository
(https://graphics.stanford.edu/data/3Dscanrep/), vrip reconstruction
(lucy.tar.gz, 307 MB compressed, 533 MB uncompressed; keep renderings of
the statue in good taste, per the repository terms). The file is a
binary big endian PLY with 14,027,872 vertices and 28,055,742
position-only faces; LuxCore's PLY reader handles the endianness
natively.

Reference numbers for a quick sanity check (simplify2 target 0.1, 20
threads): the scene simplifies 28,055,742 down to 5,859,336 faces in
about 23 secs - the run ends on the 64 iteration cap, before the
requested target. The rendered image is written to image.png (the
deprecated image.filename property is ignored by the film output
parsing).
