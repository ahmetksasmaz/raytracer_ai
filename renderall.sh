#!/bin/bash

make release

cd build/hw6

cd brdf/inputs
rm *.exr
rm *.png

../../../../raytracer killeroo_blinnphong.json
../../../../raytracer killeroo_torrancesparrow.json

cp *.exr ../../../../final_outputs/.
cp *.png ../../../../final_outputs/.

cd ../..

cd directLighting/inputs
rm *.exr
rm *.png

../../../../raytracer cornellbox_jaroslav_diffuse_area.json
../../../../raytracer cornellbox_jaroslav_glossy_area_small.json
../../../../raytracer cornellbox_jaroslav_glossy.json
../../../../raytracer cornellbox_jaroslav_diffuse.json
../../../../raytracer cornellbox_jaroslav_glossy_area_sphere.json
../../../../raytracer cornellbox_jaroslav_glossy_area_ellipsoid.json
../../../../raytracer cornellbox_jaroslav_glossy_area.json

cp *.exr ../../../../final_outputs/.
cp *.png ../../../../final_outputs/.

cd ../..

cd pathTracing/inputs
rm *.exr
rm *.png

../../../../raytracer cornell_diffuse.json
../../../../raytracer cornell_glass_mirror.json
../../../../raytracer cornellbox_prism_light.json
../../../../raytracer cornellbox_sphere_light.json

cp *.exr ../../../../final_outputs/.
cp *.png ../../../../final_outputs/.