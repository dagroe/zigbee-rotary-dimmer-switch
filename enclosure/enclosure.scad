$fa = 1;
$fs = 0.2;



make_shaft = true;
make_base_plate = true;
make_spacer = true;
make_middle_spacer = true;
build_bottom_shell = true;
render_top_pcb = false;
render_bottom_pcb = false;

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
        switch_cutout_offset_y = -16.2;
        translate([0, switch_cutout_offset_y, 0]) linear_extrude(thickness+2, center=true) square([switch_cutout_width, switch_cutout_depth], center=true);
    }
}


// make custom shaft
shaft_length_encoder_connector = 8;
shaft_length_below_button_disc = 5.6;
// TOD add offset for disc from bottom / split bottom into half-shaft connector and full-shaft
shaft_length_top = 20;
shaft_length = shaft_length_encoder_connector + shaft_length_below_button_disc + shaft_length_top;
shaft_diameter_top = 4.0;
shaft_radius_top = shaft_diameter_top/2;
shaft_diameter_encoder_connector = 3.1;
shaft_radius_encoder_connector = shaft_diameter_encoder_connector/2;
shaft_encoder_connector_cut_offset_y = 0.3;

disc_height = 1;
disc_diameter = 6;
disc_radius = disc_diameter/2;

shaft_4mm_part_length = shaft_length_below_button_disc + shaft_length_top;


if(make_shaft) {

translate([0,0,0]) color([1,0,0]) union() {
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
}

flap_thickness = 0.8;

spacer_height = 2.6 + flap_thickness; //2.6
cutter_height = spacer_height + 2;


// make spacer between base plate and top pcb that also stabilizes the push ring
if(make_spacer) {
    translate([0,0,10.2]) color([0,0,1]) {
        union() {
        difference() {
            // base rectangle
            translate([0,4,0]) linear_extrude(height=spacer_height) square([29.6,25], center=true);
            // subtract first hole for encoder clamp
            translate([6,0,cutter_height/2-1]) cylinder(h=cutter_height, r=1.5, center=true);
            // subtract second for encoder clamp
            translate([-6,0,cutter_height/2-1]) cylinder(h=cutter_height, r=1.5, center=true);
            // subtract cutout for push disc
            //translate([0,0,cutter_height/2+1.8]) cylinder(h=cutter_height, r=11/2, center=true);
            // subtract cutout for button
            translate([0,4,-0.5]) linear_extrude(height=cutter_height) square([9.8,5], center=true);
            // subtract cutout for LED
            translate([-12,4.7,-0.5]) linear_extrude(height=cutter_height) square([8,8], center=true);
            // subtract cutout for buttons
            translate([-10,14.5,-0.5]) linear_extrude(height=cutter_height) square([14,12], center=true);
            // flap cutout
            translate([0,0,-1]) linear_extrude(height=spacer_height+2) square([9.8,12], center=true);
            // holes for encoder pins
            translate([0,-7.5,0]) cylinder(h=4, r=1, center=true);
            translate([0,-8.5,-2]) linear_extrude(height=4) square([2,2], center=true);
            translate([-2.54,-7.5,0]) cylinder(h=4, r=1, center=true);
            translate([-2.54,-8.5,-2]) linear_extrude(height=4) square([2,2], center=true);
            translate([2.54,-7.5,0]) cylinder(h=4, r=1, center=true);
            translate([2.54,-8.5,-2]) linear_extrude(height=4) square([2,2], center=true);
        }
        difference() {
            // make "flap"
            translate([0,-1,spacer_height-flap_thickness]) linear_extrude(height=flap_thickness) square([8.8,13.8], center=true);
            // subtract hole for shaft
            translate([0,0,cutter_height/2-1]) cylinder(h=cutter_height, r=shaft_radius_top+0.2, center=true);
        }
        // make pusher on flap
        translate([0,4,spacer_height-flap_thickness-0.2]) linear_extrude(height=0.2) square([5,3], center=true);
       
        }
    }
    
}

// make spacer between top and bottom pcb
if(make_middle_spacer) {
    middle_spacer_base_plate_height = 2;
    pin_cover_height = 3;
    // plate
    color([0.5,0.5,0]) translate([0,2,-2.3]) difference() {
        union() {
             // plate filling shell
             linear_extrude(height=middle_spacer_base_plate_height) offset(r=-0.2) {import(file = "dimmer_esp_edges.svg", center = true, dpi = 72);}
             // fill between USB and power connector
            translate([-15.2,-12,0.2]) linear_extrude(height=3) square([9.47, 8], center=true);
            // fill above ESP
            translate([8,12,-1.7+2.3]) linear_extrude(height=7.65) square([9, 11], center=true);
            // cover for holes for pins
            translate([-13.8,0.2,middle_spacer_base_plate_height]) cylinder(h=2, r=2, center=true);
            translate([-11.2,15.2,middle_spacer_base_plate_height]) cylinder(h=2, r=2, center=true);
            translate([13.7,-4.6,middle_spacer_base_plate_height]) cylinder(h=2, r=2, center=true);
            translate([15,2.8,middle_spacer_base_plate_height]) cylinder(h=2, r=2, center=true);
         }
         // holes for pins power
        translate([-13.8,0.2,middle_spacer_base_plate_height/2-1]) cylinder(h=middle_spacer_base_plate_height+2, r=1, center=true);
        translate([-11.2,15.2,middle_spacer_base_plate_height/2-1]) cylinder(h=middle_spacer_base_plate_height+2, r=1, center=true);
        translate([13.7,-4.6,middle_spacer_base_plate_height/2-1]) cylinder(h=middle_spacer_base_plate_height+2, r=1, center=true);
        translate([15,2.8,middle_spacer_base_plate_height/2-1]) cylinder(h=middle_spacer_base_plate_height+2, r=1, center=true);
         // holes for pins connector
        translate([-13,-11.05,middle_spacer_base_plate_height/2]) cylinder(h=middle_spacer_base_plate_height+1, r=1.5, center=true);
        translate([-13-(2*2.54),-11.05,middle_spacer_base_plate_height/2]) cylinder(h=middle_spacer_base_plate_height+1, r=1.5, center=true);
        // hole for connector between boards
        translate([15.2,-14,-1]) linear_extrude(height=middle_spacer_base_plate_height+2) square([6, 6], center=true);
        // hole for encoder
        translate([0,-2,middle_spacer_base_plate_height+3/2-1]) cylinder(h=3, r=4, center=true); 
    }
}
// TODO: remove this visualization for space between pcbs
//space_between_top_and_bottom_pcb = 10.9;
//color([1,0,0]) translate([0,64,-2.3]) linear_extrude(height=space_between_top_and_bottom_pcb) square([6, 6], center=true);

// parameters for shell and front plate
// dimensions for shell
shell_wall_thickness = 2.5;
shell_bottom_thickness = 1.5;
space_at_bottom_of_shell = 0;
height_of_pcb_components_bottom = 17-1.6;
bottom_pcb_thickness = 1.6;
height_of_pcb_components_middle = 10.9; //7.5;
top_pcb_thickness = 1.6;

front_button_height = 3.0;
space_between_top_pcb_and_base_plate = spacer_height;//front_button_height + disc_height;

// base plate parameters
base_plate_thickness = 1.6;
base_plate_width_and_depth = 71.0;
base_plate_corner_radius = 6.0;

// socket adapter and spacer parameters
side_thickness = 2.0;

shell_height = shell_bottom_thickness + space_at_bottom_of_shell + height_of_pcb_components_bottom + bottom_pcb_thickness + height_of_pcb_components_middle + top_pcb_thickness + space_between_top_pcb_and_base_plate;

offset_base_plate = shell_height - shell_bottom_thickness - base_plate_thickness;

top_rod_height = 7;

reinforcement_ring_diameter = 14;
reinforcement_ring_radius = reinforcement_ring_diameter / 2;
reinforcement_ring_height = 1.2;

base_plate_offset_z = 13.6;

if(make_base_plate) {

translate([0,0,base_plate_offset_z]) difference() {
    union() {
        // base plate with cutouts and hole for switch, LED, etc.
       difference() {
            base_plate(base_plate_thickness, base_plate_width_and_depth, base_plate_corner_radius, offset_base_plate);
            translate([-12,4.7,5-0.5]) cylinder(h=10, r=2, center=true);
            // subtract cutout for buttons
            translate([-11.5,14.5,5-0.5]) cylinder(h=10, r=2, center=true);
            translate([-5.5,14.5,5-0.5]) cylinder(h=10, r=2, center=true);
        }
        
        
        // connector left side
        translate([0,0,-spacer_height]) difference() {
            union() {
                translate([-18,8.5,0]) linear_extrude(height=spacer_height+1) square([6,34], center=true);
                translate([-16.5,-10.5,0]) linear_extrude(height=spacer_height+1) square([9,4], center=true);
            }
            translate([-(18.5+10.5),18.5+10.5,-2]) rotate([0,0,45]) linear_extrude(height=spacer_height+4) square([33,33], center=true);
            translate([-17,15,spacer_height/2]) cylinder(h=spacer_height+2, r = 1, center=true);
            // holes for screws
            translate([0,0,1.5]) rotate([0,90,0]) cylinder(h=50, r=0.5, center=true); 
        }
        
        // connector right side
        translate([0,0,-spacer_height]) difference() {
            union() {
                translate([18,7.5,0]) linear_extrude(height=spacer_height+1) square([6,34], center=true);
                translate([10,18.5,0]) linear_extrude(height=spacer_height+1) square([26,4], center=true);
            }
            translate([18.5+10.5,18.5+10.5,-2]) rotate([0,0,45]) linear_extrude(height=spacer_height+4) square([33,33], center=true);
            // holes for screws
            translate([0,0,1.5]) rotate([0,90,0]) cylinder(h=50, r=0.5, center=true); 
        }
        
        // reinforcment ring around bushing
        translate([0,0,base_plate_thickness + reinforcement_ring_height/2]) cylinder(h=reinforcement_ring_height, r=reinforcement_ring_radius, center=true);
        
        // thread / bushing
        intersection() {
            linear_extrude(top_rod_height+base_plate_thickness) square([11,11], center=true);
            threaded_rod(d=10, l=top_rod_height+20, pitch=1.5, left_handed=false, $fa=1, $fs=1);
        }
    }
    
    // hole for shaft in center
    cylinder(h=60, r=(shaft_diameter_top+0.2)/2, center=true);
    
    // hole for disc that pushes on the button
    translate([0,0,disc_height/2-0.5]) cylinder(h=disc_height+2, r=disc_radius+0.1, center=true);
}
}


space_between_shell_and_PCB = 0.3;


if(build_bottom_shell) {

color([1,0,0]) translate([0,1.5,base_plate_offset_z-shell_height]) difference() {
    linear_extrude(height=shell_height) offset(r=(space_between_shell_and_PCB+shell_wall_thickness)/2) hull() {
        import(file = "dimmer_esp_edges.svg", center = true, dpi = 72);
    }

    translate([0,0,shell_bottom_thickness]) linear_extrude(height=shell_height) offset(r=space_between_shell_and_PCB/2) hull() {import(file = "dimmer_esp_edges.svg", center = true, dpi = 72);}
    
    // hole for mains wires
    translate([-15.5,-16-1.5,-11-(base_plate_offset_z-shell_height)]) linear_extrude(6) square([10,9], center=true);
    // hole for screws of mains wires connector
    translate([-15.5,-9.5-1.5,-22-(base_plate_offset_z-shell_height)]) linear_extrude(6) square([10,5], center=true);
    // hole for switch
    translate([0,-20,shell_height-space_between_top_pcb_and_base_plate]) linear_extrude(space_between_top_pcb_and_base_plate+1) square([22,5], center=true);
    // holes for screws
    translate([0,-1.5,shell_height-2]) rotate([0,90,0]) cylinder(h=50, r=1, center=true); 
}
    
}




/*
translate([0,0,10]) color([0,1,0]) {
difference() {
translate([0,0,2.5]) cylinder(h=1.8, r=6, center=true);
translate([6,0,2.5]) cylinder(h=6, r=2, center=true);
translate([-6,0,2.5]) cylinder(h=6, r=2, center=true);
translate([0,4,-0.5]) linear_extrude(height=6) square([9,5], center=true);
translate([-11,4.5,-0.5]) linear_extrude(height=6) square([6,6], center=true);
translate([0,0,2.5]) cylinder(h=6, r=shaft_radius_top, center=true);
}
}
*/

//translate([-16.95,15.05,3]) color([0,0,1]) cylinder(h=12,r=1, center=true);

if(render_top_pcb) {
    translate([0,1.3,0]) import("../hardware/3d/dimmer_esp_v1_4_main.stl", convexity=3);
    
    // make dummy USB
    translate([-15.2,-10,0.9]) color([1,1,0]) linear_extrude(height=7.65) square([9.47, 8], center=true);
}

if(render_bottom_pcb) {
translate([0,2,-19]) color([0,1,0]) import("../hardware/3d/dimmer_esp_v1_4_power230.stl", convexity=3);
}
