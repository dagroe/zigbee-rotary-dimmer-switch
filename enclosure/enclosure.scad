$fa = 1;
$fs = 0.2;

include <BOSL2/std.scad>
include <BOSL2/threading.scad>

// function for rounded rectangle
module make_rounded_rectangle(width, height, radius) {
    hull() {
        translate([radius,radius,0]) circle(radius);
        translate([width-radius,radius,0]) circle(radius);
        translate([width-radius,height-radius,0]) circle(radius);
        translate([radius,height-radius,0]) circle(radius);
    }
}

// main function to make front facing plate with thread/bushing
module base_plate(thickness, width_and_depth, corner_radius, offset_z) {
    // make cutouts into plate

    // make for holes with circular slits to attach base plate to wall socket
    // prepare rectangular cutouts on border (will be rotated to four sides along other holes)
    cutout_depth_x = 1.5;
    cutout_width_y = 4.15;
    cutout_offset_y = 19.8;

    distance_from_border = 3.7;
    length_hole_in_arc_degree = 25;
    width_hole = 3.7;
    width_bigger_hole = 5.5;
    arc_offset = width_hole * 0.5;

    radius_circle = width_and_depth*0.5 - distance_from_border - arc_offset;

    difference(){
        // make solid plate first
        linear_extrude(height=thickness) translate([-width_and_depth*0.5, -width_and_depth*0.5, 0]) make_rounded_rectangle(width_and_depth, width_and_depth, corner_radius);

        // make cutouts on 4 sides
        for (i=[0:1:4]) {
            center = i * 360 / 4;

            // holes for screws
            rotate([0, 0, center-length_hole_in_arc_degree/2]) rotate_extrude(angle=length_hole_in_arc_degree,convexity = 10) translate([radius_circle, 0,0]) square(width_hole, center=true);
            rotate([0, 0, center+length_hole_in_arc_degree/2]) linear_extrude(5, center=true) translate([radius_circle, 0, 0]) circle(d=width_bigger_hole);
            rotate([0, 0, center-length_hole_in_arc_degree/2]) linear_extrude(5, center=true) translate([radius_circle, 0, 0]) circle(d=width_hole);

            // hole markers on sides
            rotate([0, 0, center]) translate([width_and_depth*0.5 - cutout_depth_x, cutout_offset_y*0.5+cutout_width_y*0.5, 0]) translate([0, -cutout_width_y*0.5, 0]) linear_extrude(20, center=true) square([cutout_depth_x, cutout_width_y]);
            rotate([0, 0, center]) translate([width_and_depth*0.5 - cutout_depth_x, -(cutout_offset_y*0.5+cutout_width_y*0.5), 0]) translate([0, -cutout_width_y*0.5, 0]) linear_extrude(20, center=true) square([cutout_depth_x, cutout_width_y]);

        }

        // cutout rectangle for switch
        switch_cutout_width = 18.0;
        switch_cutout_depth = 7.0;
        switch_cutout_offset_y = -14.2;
        translate([0, switch_cutout_offset_y, 0]) linear_extrude(thickness+2, center=true) square([switch_cutout_width, switch_cutout_depth], center=true);
    }
}


// make custom shaft
shaft_length_encoder_connector = 7;
shaft_length_below_button_disc = 6;
// TOD add offset for disc from bottom / split bottom into half-shaft connector and full-shaft
shaft_length_top = 14;
shaft_length = shaft_length_encoder_connector + shaft_length_below_button_disc + shaft_length_top;
shaft_diameter_top = 4.0;
shaft_radius_top = shaft_diameter_top/2;
shaft_diameter_encoder_connector = 3.1;
shaft_radius_encoder_connector = shaft_diameter_encoder_connector/2;
shaft_encoder_connector_cut_offset_y = 0.3;

disc_height = 1.5;
disc_diameter = 10;
disc_radius = disc_diameter/2;

shaft_4mm_part_length = shaft_length_below_button_disc + shaft_length_top;

color([1,0,0]) union() {
    // make top shaft part (4mm diameter)
    translate([0,0,shaft_length_encoder_connector+shaft_4mm_part_length/2]) cylinder(h=shaft_4mm_part_length, r=shaft_radius_top, center=true);
    // make disc that pushes on the button
    translate([0,0,shaft_length_encoder_connector+shaft_length_below_button_disc+(disc_height/2)]) cylinder(h=disc_height, r=disc_radius, center=true);
    // make encoder connector (cut off half the shaft for "D" shape)
    difference() {
        translate([0,0,shaft_length_encoder_connector/2]) cylinder(h=shaft_length_encoder_connector, r=shaft_radius_encoder_connector, center=true);
        translate([-(shaft_radius_encoder_connector+1), shaft_encoder_connector_cut_offset_y, -1]) cube([shaft_diameter_encoder_connector+2, shaft_radius_encoder_connector+1, shaft_length_encoder_connector+2]);
    }
}

// parameters for shell and front plate
// dimensions for shell
shell_wall_thickness = 1.5;
shell_bottom_thickness = 1.5;
space_at_bottom_of_shell = 0;
height_of_pcb_components_bottom = 15.1;
bottom_pcb_thickness = 1.6;
height_of_pcb_components_middle = 7.5;
top_pcb_thickness = 1.6;

front_button_height = 3.0;
space_between_top_pcb_and_base_plate = front_button_height + disc_height;

// base plate parameters
base_plate_thickness = 1.6;
base_plate_width_and_depth = 71.0;
base_plate_corner_radius = 6.0;

// socket adapter and spacer parameters
side_thickness = 2.0;

shell_height = shell_bottom_thickness + space_at_bottom_of_shell + height_of_pcb_components_bottom + bottom_pcb_thickness + height_of_pcb_components_middle + top_pcb_thickness + space_between_top_pcb_and_base_plate;

offset_base_plate = shell_height - shell_bottom_thickness - base_plate_thickness;

top_rod_height = 5;

reinforcement_ring_diameter = 14;
reinforcement_ring_radius = reinforcement_ring_diameter / 2;
reinforcement_ring_height = 1.2;

base_plate_offset_z = 15;

translate([0,0,base_plate_offset_z]) difference() {
    union() {
        // base plate with cutouts and hole for switch
        base_plate(base_plate_thickness, base_plate_width_and_depth, base_plate_corner_radius, offset_base_plate);
        
        // reinforcment ring around bushing
        translate([0,0,base_plate_thickness + reinforcement_ring_height/2]) cylinder(h=reinforcement_ring_height, r=reinforcement_ring_radius, center=true);
        
        // thread / bushing
        intersection() {
            linear_extrude(top_rod_height+base_plate_thickness) square([11,11], center=true);
            threaded_rod(d=10, l=top_rod_height+10, pitch=1.5, left_handed=false, $fa=1, $fs=1);
        }
    }
    
    // hole for shaft in center
    cylinder(h=60, r=(shaft_diameter_top+0.1)/2, center=true);
}

space_between_shell_and_PCB = 0.1;

color([1,0,0]) translate([0,1.5,base_plate_offset_z-shell_height]) difference() {
    linear_extrude(height=shell_height) offset(r=(space_between_shell_and_PCB+shell_wall_thickness)/2) hull() {
        import(file = "dimmer_esp_edges.svg", center = true, dpi = 72);
    }

    translate([0,0,shell_bottom_thickness]) linear_extrude(height=shell_height) offset(r=space_between_shell_and_PCB/2) hull() {import(file = "dimmer_esp_edges.svg", center = true, dpi = 72);}
}


//translate([-16.95,15.05,3]) color([0,0,1]) cylinder(h=12,r=1, center=true);


translate([0,2,0]) import("../hardware/3d/dimmer_esp_v1_4_main.stl", convexity=3);
translate([0,2,-15]) color([0,1,0]) import("../hardware/3d/dimmer_esp_v1_4_power230.stl", convexity=3);
