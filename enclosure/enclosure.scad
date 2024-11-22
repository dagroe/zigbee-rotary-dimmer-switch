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

// parameters for shell and front plate
// dimensions for shell
shell_wall_thickness = 1.5;
shell_bottom_thickness = 1.5;
space_at_bottom_of_shell = 0;
height_of_pcb_components_bottom = 15.05;
bottom_pcb_thickness = 1.6;
height_of_pcb_components_middle = 5.87;
top_pcb_thickness = 1.6;
space_between_top_pcb_and_base_plate = 10.0;

// base plate parameters
base_plate_thickness = 1.6;
base_plate_width_and_depth = 71.0;
base_plate_corner_radius = 6.0;

// socket adapter and spacer parameters
side_thickness = 2.0;

shell_height = shell_bottom_thickness + space_at_bottom_of_shell + height_of_pcb_components_bottom + bottom_pcb_thickness + height_of_pcb_components_middle + top_pcb_thickness + space_between_top_pcb_and_base_plate;

offset_base_plate = shell_height - shell_bottom_thickness - base_plate_thickness;

top_rod_height = 10;


translate([0,0,15]) difference() {
    union() {
        base_plate(base_plate_thickness, base_plate_width_and_depth, base_plate_corner_radius, offset_base_plate);
        intersection() {
            linear_extrude(top_rod_height) square([11,11], center=true);
            threaded_rod(d=10, l=top_rod_height+10, pitch=1.5, left_handed=false, $fa=1, $fs=1);
        }
    }
    cylinder(h=60, r=2.1, center=true);
}

// make custom shaft
shaft_length_bottom = 10;
shaft_length_top = 25;
shaft_length = shaft_length_bottom + shaft_length_top;
shaft_diameter_top = 4.0;
shaft_radius_top = shaft_diameter_top/2;
shaft_diameter_bottom = 3.1;
shaft_radius_bottom = shaft_diameter_bottom/2;
shaft_cut_offset_y = 0.3;

disc_height = 1.5;
disc_diameter = 10;
disc_radius = disc_diameter/2;


color([1,0,0]) union() {
    translate([0,0,shaft_length_bottom+shaft_length_top/2]) cylinder(h=shaft_length_top, r=shaft_radius_top, center=true);
    translate([0,0,shaft_length_bottom+(disc_height/2)]) cylinder(h=disc_height, r=disc_radius, center=true);
    difference() {
        translate([0,0,shaft_length_bottom/2]) cylinder(h=shaft_length_bottom, r=shaft_radius_bottom, center=true);
        translate([-(shaft_radius_bottom+1), shaft_cut_offset_y, -1]) cube([shaft_diameter_bottom+2, shaft_radius_bottom+1, shaft_length_bottom+2]);
    }
}