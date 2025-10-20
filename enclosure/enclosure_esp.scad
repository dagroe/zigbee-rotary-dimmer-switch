$fa = 1;
$fs = 0.2;
$fn=64;



make_shaft = true;
make_base_plate = true;
make_thread = false;
make_spacer = false;
make_middle_spacer = true;
build_bottom_shell = true;
render_pcb = true;

include <BOSL2/std.scad>
include <BOSL2/threading.scad>

//translate([0.3,-0.8,0]) import(file="esp_edge_cuts3.svg",center=true);

/*
points = [
[181.5000,39.0000],
[189.5000,46.0000],
[189.5000,76.0000],
[181.5000,83.0000],
[172.0000,86.5000],
[163.5000,86.5000],
[153.5000,83.0000],
[145.5000,76.0000],
[145.5000,46.0000],
[153.5000,39.0000],
[157.5000,37.0000],
[177.5000,37.0000]
];

paths = [[0,1,2,3,4,5,6,7,8,9,10,11,0]];

difference() {
translate([-180,0,0]) linear_extrude(20) offset(1) polygon(points=points, paths=paths);
translate([-180,0,-1]) linear_extrude(22) polygon(points=points, paths=paths);
}
*/

module my_model() {

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

            // cutout rectangle for USB
            switch_cutout_width = 3.16 + 0.3;
            switch_cutout_depth = 8.94 + 0.3;
            switch_cutout_offset_x = 12.95 - switch_cutout_width/2;
            switch_cutout_offset_y = -17.2 - switch_cutout_depth/2;
            translate([switch_cutout_offset_x, switch_cutout_offset_y, 0]) linear_extrude(thickness+2, center=true) make_rounded_rectangle(switch_cutout_width, switch_cutout_depth, 1);
        }
    }

    // socket adapter
    module SocketAdapter(side_thickness=1.0, shaft_radius=3.0, offset_z=0) {

        // ----- Parameters -----
        top_thickness       = 0.5;
        groove_margin       = 0.10;
        height              = 4 + 0.2; // knob height + margin
        thread_tunnel_margin= 0.2;
        thread_tunnel_length= 3.5;
        dial_connector_length = 4.2;
        margin_from_knob    = 0.2;
        overlap_bottom      = 0.1;
        knob_radius         = 3.0;

        // ----- Inner cutout -----
        module knob_socket_make_inner_cutout(knob_radius, margin_from_knob, height) {
            outer_r = knob_radius + margin_from_knob;
            inner_r = outer_r - 0.5;
            n = 36; // 2*18
            pts = [
                for (i = [0:n-1]) 
                    let (a = 360/n * i)
                    [(i % 2 == 0 ? inner_r : outer_r) * cos(a),
                     (i % 2 == 0 ? inner_r : outer_r) * sin(a)]
            ];
            linear_extrude(height)
                polygon(points=pts);
        }

        // ----- Outer shell -----
        module knob_socket_make_outer_shell(knob_radius, side_thickness, top_thickness, height) {
            difference() {
                cylinder(r=knob_radius+side_thickness, h=height+top_thickness, $fn=72);
            }
        }

        // ----- Groove -----
        module knob_socket_make_groove(shaft_radius, groove_width, groove_height, height) {
            width = shaft_radius*2-0.2;
            translate([-width/2, -groove_width/2, height-groove_height])
                cube([width, groove_width, groove_height], center=false);
        }

        // ----- Knob socket -----
        module make_knob_socket(knob_radius, shaft_radius, margin_from_knob, side_thickness, top_thickness, groove_margin, height) {
            groove_width  = 1 - groove_margin;
            groove_height = 1.6;
            
            union() {
                difference() {
                    knob_socket_make_outer_shell(knob_radius, side_thickness, top_thickness, height);
                    translate([0,0, -1]) knob_socket_make_inner_cutout(knob_radius, margin_from_knob, height + 1);
                }
                knob_socket_make_groove(shaft_radius, groove_width, groove_height, height);
            }
            
        }

        // ----- Shaft -----
        module make_shaft(height, top_thickness, shaft_radius, thread_tunnel_length, dial_connector_length, overlap_bottom) {
            overlap_top = 0.5;

            // Cross boxes
            box_width = 1;
            box_height = dial_connector_length+overlap_top;
            
            middle_radius = shaft_radius-0.6;

            union() {
                translate([0,0,-overlap_bottom]) cylinder(h=thread_tunnel_length+overlap_top+overlap_bottom, r=middle_radius);
                translate([0,0,thread_tunnel_length])
                union() {
                    translate([0,0,box_height/2]) cube([box_width, middle_radius*2 - 0.1, box_height], center=true);
                    translate([0,0,box_height/2]) cube([middle_radius*2 - 0.1, box_width, box_height], center=true);
                    cylinder(r=2.05, h=dial_connector_length+overlap_top);
               };
            }
            
        }
        


        // ----- Build adapter -----
        translate([0,0, offset_z - (height + top_thickness)])
            union() {
                make_knob_socket(knob_radius, shaft_radius, margin_from_knob, side_thickness, top_thickness, groove_margin, height);
                translate([0,0,height+top_thickness-overlap_bottom])
                     make_shaft(height, top_thickness, shaft_radius, thread_tunnel_length, dial_connector_length, overlap_bottom);
            }
            
    }


    side_thickness = 0.8;
        
    // socket adapter, thread tunnel cutout, and spacer
    shaft_diameter = 7.0;
    shaft_radius = shaft_diameter / 2;

    offset_z = 2;

    if (make_shaft) {
        color([1,0,0]) translate([0,0,-1.5]) SocketAdapter(side_thickness, shaft_radius, offset_z);
        
    }

    // Make caps to cover 230V pins on front side
    space_between_plate_and_pcb = 11.0;
    
    connector_pin_offset_x = 0.75;
    connector_pin_offset_y = -19.95+0.76;
    connector_pin_offset_z = -6.0-space_between_plate_and_pcb;
    connector_pin_radius = 1;
    connector_pin_height = 2.5;

    if(make_middle_spacer) {
    
        // cover for 230V connector pin ends
        /*
        for (i=[-1:1:1]) {
            center_x = connector_pin_offset_x+2.54*2*i;
            color([1,1,0]) difference() {
                translate([center_x,connector_pin_offset_y,connector_pin_offset_z]) cylinder(h=space_between_plate_and_pcb,r=connector_pin_radius+1);
                translate([center_x,connector_pin_offset_y,connector_pin_offset_z-1]) cylinder(h=connector_pin_height+1,r=connector_pin_radius);
            }
        }
        */
        
        // connector for screws
        color([1,0,0]) difference() {
            translate([18.56,-10.52,connector_pin_offset_z]) cylinder(h=space_between_plate_and_pcb,r=2);
            translate([18.56,-10.52,connector_pin_offset_z-1]) cylinder(h=5,r=1);
        }
        
        
        color([1,0,0]) difference() {
            translate([-16.65,16.54,connector_pin_offset_z]) cylinder(h=space_between_plate_and_pcb,r=2);
            translate([-16.65,16.54,connector_pin_offset_z-1]) cylinder(h=5,r=1);
        }
        
        
    }


    // base plate parameters
    base_plate_thickness = 1.6;
    base_plate_width_and_depth = 71.0;
    base_plate_corner_radius = 6.0;



    //shell_height = shell_bottom_thickness + space_at_bottom_of_shell + height_of_pcb_components_bottom + bottom_pcb_thickness + height_of_pcb_components_middle + top_pcb_thickness + space_between_top_pcb_and_base_plate;

    offset_base_plate = 0;//shell_height - shell_bottom_thickness - base_plate_thickness;

    top_rod_height = 7;

    reinforcement_ring_diameter = 14;
    reinforcement_ring_radius = reinforcement_ring_diameter / 2;
    reinforcement_ring_height = 1.2;

    base_plate_offset_z = -7;//13.6;


    //color([1,0,0]) translate([0,0,5]) cylinder(h=20, r=2.9, center=true);

    // shaft adapter

    thread_pitch = 1;
    thread_diameter = 10;
    
    height_thread_connector_including_base_plate = 4.0;
    
    // connectors on base plate whern shell can be fixed with crews
    shell_connector_height = 4.6;
    shell_connector_width = 3.0;
    shell_connector_depth = 16.0;
    shell_side_screw_diameter = 1.5;
    
    button_hole_diameter = 3.0;

    if(make_base_plate) {
    
    

         translate([0,0,base_plate_offset_z]) difference() {
            union() {
                // base plate with cutouts and hole for switch, LED, etc.
               
               difference() {
                    base_plate(base_plate_thickness, base_plate_width_and_depth, base_plate_corner_radius, offset_base_plate);
                    //translate([-12,4.7,5-0.5]) cylinder(h=10, r=2, center=true);
                    
                    // subtract cutout for buttons
                    
                    translate([-12.3,-1.4,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
                    translate([-4.1,-11.0,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
                    translate([14.3,-3.4,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
                    // hole for shaft in center
                    translate([0,0,10]) cylinder(h=60, r=(thread_diameter/2+0.5), center=true);
                }
                
                
                
                
                // reinforcment ring around bushing
                translate([0,0,base_plate_thickness + reinforcement_ring_height/2]) cylinder(h=reinforcement_ring_height, r=reinforcement_ring_radius, center=true);
                
                
                // thread / bushing
                /*
                intersection() {
                    translate([0,0,base_plate_thickness-height_thread_connector_including_base_plate]) linear_extrude(top_rod_height+height_thread_connector_including_base_plate) square([11,11], center=true);
                    threaded_rod(d=thread_diameter, l=top_rod_height+30, pitch=thread_pitch, left_handed=false, blunt_start=false, $fa=5, $fs=5);
                }
                */
                
                translate([0,0,height_thread_connector_including_base_plate-0.9]) threaded_rod(d=thread_diameter, l=top_rod_height+height_thread_connector_including_base_plate, pitch=thread_pitch, left_handed=false, blunt_start2=true, blunt_start1=false, $fa=5, $fs=5);
                
                // reinforcement ring around encoder
                translate([0,0,-2.6]) cylinder(h=4.2, r=shaft_radius+4.0+0.1);
                
                // connectors on base plate where shell can be fixed with screws                
                difference() {
                    translate([20.98,0,-shell_connector_height]) linear_extrude(shell_connector_height+base_plate_thickness) square([shell_connector_width,shell_connector_depth], center=true);
                    translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-60]) cylinder(r=shell_side_screw_diameter/2, h=120);
                }
                
                difference() {
                    translate([-20.38,0,-shell_connector_height]) linear_extrude(shell_connector_height+base_plate_thickness) square([shell_connector_width,shell_connector_depth], center=true);
                    translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-60]) cylinder(r=shell_side_screw_diameter/2, h=120);
                }
                
                // inside thread
                //translate([0,0,-0.4]) threaded_nut(nutwidth=16, id=thread_diameter, h=height_thread_connector_including_base_plate, pitch=thread_pitch, shape="square", bevel=false, blunt_start=false, $slop=0.05, $fa=1, $fs=1);                                
            }  
  

            // hole for shaft in center
            translate([0,0,10]) cylinder(h=60, r=(shaft_radius-0.5+0.2), center=true);
            
            // hole for knob wrapper
            translate([0,0,-3.4]) cylinder(h=11, r=shaft_radius+0.4);
            
            
            // hole for encoder body
            translate([0,0,-3.4]) cylinder(h=5, r=shaft_radius+0.6);  
        }
    }
    
    if(make_thread) {
        translate([0,0,base_plate_offset_z+0.0]) difference() {
            union() {           
                // reinforcment ring around bushing
                //translate([0,0,base_plate_thickness + reinforcement_ring_height/2]) cylinder(h=reinforcement_ring_height, r=reinforcement_ring_radius, center=true);
                
                // thread / bushing
                intersection() {
                    translate([0,0,base_plate_thickness-height_thread_connector_including_base_plate]) linear_extrude(top_rod_height+height_thread_connector_including_base_plate) square([11,11], center=true);
                    threaded_rod(d=thread_diameter, l=top_rod_height+30, pitch=thread_pitch, left_handed=false, $fa=5, $fs=5);
                }
                
                // reinforcment ring around encoder
                //translate([0,0,-2.4]) cylinder(h=4.5, r=shaft_radius+2.0+0.1);
            }
            
            // hole for shaft in center
            translate([0,0,10]) cylinder(h=60, r=(shaft_radius-0.5+0.2), center=true);
            
            // hole for knob wrapper
            translate([0,0,-3.4]) cylinder(h=11, r=shaft_radius+0.4);
            
            
            // hole for encoder body
            translate([0,0,-3.4]) cylinder(h=5, r=shaft_radius+0.6);
        }
    
    }
    
    
    bottom_shell_height = 29;
    bottom_shell_thickness = 1;
    
    if (build_bottom_shell) {
    
        translate([0,0,0]) difference() {
            translate([0,0.76, -bottom_shell_height-7]) difference() {
                translate([0.3,-0.8,0]) linear_extrude(bottom_shell_height) offset(1) import(file="esp_edge_cuts_v2.svg",center=true);
                translate([0.3,-0.8,bottom_shell_thickness]) linear_extrude(bottom_shell_height+1) offset(0.2) import(file="esp_edge_cuts_v2.svg",center=true);
            }
            
            // cut out for power connector
            power_connector_hole_width = 4.0;
            for (i=[-1:1:1]) {
                power_connector_x = 0.7 + i * (power_connector_hole_width + 1);
                translate([power_connector_x,-22,-26.0]) linear_extrude(6.0) square([power_connector_hole_width,10.0], center=true);
                translate([power_connector_x,connector_pin_offset_y,-48.0]) cylinder(r=power_connector_hole_width/2, h=30.0);
        }
        
        translate([0,0,base_plate_offset_z]) translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-62]) cylinder(r=shell_side_screw_diameter/2, h=120);
                 
        translate([0,0,base_plate_offset_z]) translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-62]) cylinder(r=shell_side_screw_diameter/2, h=120);
        
        // hole for JST connectors
        translate([24.0,4.8,-17.4]) linear_extrude(4.0) square([10.0,8.0], center=true);
        translate([24.0,12.3,-17.4]) linear_extrude(4.0) square([10.0,5.0], center=true);
        }
        
        
    }
    
    
        
                
    /*
    translate([0,0, -9]) difference() {
        translate([0.3,-0.8,0]) linear_extrude(2) offset(1) import(file="esp_edge_cuts3.svg",center=true);
        translate([0.3,-0.8,-1]) linear_extrude(4) offset(0.2) import(file="esp_edge_cuts3.svg",center=true);
    }
    */


    if(render_pcb) {
        color([0,0,1]) translate([0.33,0,-2.6]) import("../hardware/3d/dimmer_esp_v2_main.stl", convexity=3);
    }
}

//intersection() {
my_model();
//translate([0,0,-90]) cylinder(h=680, r=7);
//translate([-20,0,-90]) linear_extrude(180) square([40,80], center=true);
//}
