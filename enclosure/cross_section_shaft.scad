// Cross section through printed shaft, thread part, base plate and encoder, to check the fit.
// Open in OpenSCAD and use the preview (F5), the colors only show there.

include <enclosure_esp.scad>

section_plane_angle = 0; // turns the cut around the axis; 0 shows the key in the encoder slot, 90 across it

// parts in assembly position
render_main_model = false;
make_shaft = true;
shaft_print_orientation = false;
make_base_plate = true;
make_base_plate_thread = true;
render_encoder = true;
make_nut = false;
build_bottom_shell = false;
render_pcb = false;

// camera: looking at the cut face
$vpr = [90, 0, section_plane_angle];
$vpt = [0, 0, -4];
$vpd = 70;

intersection() {
    my_model();
    rotate([0, 0, section_plane_angle]) translate([-50, 0, -50]) cube(100);
}
