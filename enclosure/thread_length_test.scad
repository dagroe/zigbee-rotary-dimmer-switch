// Test set: thread parts with different thread lengths, to find the longest thread that still fits under the Gira cap.
// Each part has its thread length (mm in front of the base plate) engraved on one side of the holder rect.

include <enclosure_esp.scad>

thread_lengths = [8, 9, 10, 11]; // thread length in front of the base plate, 9 is the default
part_spacing = 20;

// only the thread parts
render_main_model = false;
make_shaft = false;
make_base_plate = false;
make_base_plate_thread = true;
make_nut = false;
build_bottom_shell = false;
render_pcb = false;
render_encoder = false;

for (i = [0:len(thread_lengths)-1])
    translate([i*part_spacing, 0, 0]) my_model(thread_lengths[i], str(thread_lengths[i]));
