$fa = 1;
$fs = 0.25;
//$fn=64;



make_shaft = false;
shaft_print_orientation = true; // printed shaft upside down at the origin (knurl bore opens upwards, no support needed); false = assembly position
make_base_plate = false;
make_base_plate_thread = false;
make_thread = false;
make_spacer = false;
make_middle_spacer = false;
make_nut = false;
build_bottom_shell = true;
make_bottom_shell_labels = false; // only the L/N label inlays of the bottom shell, export as a second STL for a different AMS color
render_pcb = false;
render_encoder = false;
render_main_model = true; // set to false when this file is included by another file

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

// thread_length_above_base_plate: how far the M10 thread sticks out in front of the base plate
// thread_part_label: optional text engraved into the side of the thread part, e.g. for test prints
module my_model(thread_length_above_base_plate=9.0, thread_part_label="") {

    // ----- Encoder and printed shaft -----
    // Bourns PEC11S (SMD), L1 = 9.5 version: fixed bushing, then a short rotating shaft with a slotted knurl on top
    pec11s_z_offset = 0.4;                 // encoder sits this much above the PCB top
    pec11s_body_width = 12.5;
    pec11s_body_height = 4.5;              // measured
    pec11s_shaft_length_above_body = 9.5;  // L1, fixed bushing + rotating shaft
    pec11s_bushing_diameter = 7.0;         // fixed, does not rotate
    pec11s_bushing_length = 5.0;           // LB
    pec11s_neck_diameter = 4.5;            // rotating part between bushing and knurl, estimated from the drawing
    pec11s_knurl_diameter = 6.0;           // 18 teeth
    pec11s_knurl_length = 2.7;             // L2
    pec11s_slot_width = 1.0;
    pec11s_slot_depth = 1.6;

    // printed shaft (SocketAdapter): sleeve pressed onto the knurled encoder shaft, shaft runs through the thread to the knob
    knurl_bore = 5.8;                      // press fit on the 6 mm knurl
    adapter_encoder_gap = 0.5;             // between sleeve and the fixed encoder bushing
    encoder_bushing_clearance = 0.4;       // radial, around the fixed encoder bushing (SMD placement tolerance)
    adapter_key_clearance = 0.05;          // per side between key and encoder shaft slot
    adapter_key_width = pec11s_slot_width - 2*adapter_key_clearance;
    adapter_web_thickness = 1.0;           // closed top of the knurl bore, part of the round shaft above the sleeve
    adapter_axial_clearance = 0.2;         // between sleeve and the step in the thread bore
    adapter_sleeve_bore_extra_height = 2.0; // sleeve bore reaches this much higher, found by test fitting
    printed_shaft_diameter = 6.0;
    printed_shaft_tip_above_thread = 4.0;  // tip of the knob connector beyond the end of the thread

    // knob connector on top of the printed shaft, same as the old printed shaft
    knob_connector_length = 4.7;
    knob_connector_cross_width = 1.0;
    knob_connector_cross_span = printed_shaft_diameter - 0.1;
    knob_connector_core_diameter = 4.1;

    cut_overlap = 0.01;                    // makes cuts extend past coplanar faces

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
        }
    }

    // socket adapter: printed shaft, sleeve pressed onto the knurled encoder shaft, runs through the thread to the knob
    module SocketAdapter(side_thickness=1.0, offset_z=0) {
        sleeve_diameter = knurl_bore + 2*side_thickness;

        translate([0,0,offset_z]) {
            // sleeve on the knurled encoder shaft
            difference() {
                cylinder(d=sleeve_diameter, h=adapter_sleeve_height);
                translate([0,0,-cut_overlap]) cylinder(d=knurl_bore, h=adapter_knurl_depth+cut_overlap);
            }

            // key engaging the slot in the encoder shaft
            translate([0,0,adapter_knurl_depth-adapter_key_height/2+cut_overlap/2]) cube([knurl_bore, adapter_key_width, adapter_key_height+cut_overlap], center=true);

            // 45 degree chamfer from sleeve to shaft, so the sleeve does not overhang when printed upside down
            translate([0,0,adapter_sleeve_height-cut_overlap]) cylinder(d1=sleeve_diameter, d2=printed_shaft_diameter, h=adapter_sleeve_chamfer_height+cut_overlap);

            // round shaft through the thread, ends in a 45 degree chamfer down to the knob connector core
            translate([0,0,adapter_sleeve_height-cut_overlap]) cylinder(d=printed_shaft_diameter, h=adapter_shaft_round_length-knob_connector_chamfer_height+cut_overlap);
            translate([0,0,adapter_sleeve_height+adapter_shaft_round_length-knob_connector_chamfer_height-cut_overlap])
                cylinder(d1=printed_shaft_diameter, d2=knob_connector_core_diameter, h=knob_connector_chamfer_height+2*cut_overlap);

            // knob connector: cross with round core; the arms continue through the chamfer into the full shaft,
            // so they carry the chamfer when printed upside down
            translate([0,0,adapter_sleeve_height+adapter_shaft_round_length-cut_overlap]) {
                cross_height = knob_connector_length + knob_connector_chamfer_height + cut_overlap;
                translate([0,0,knob_connector_length+cut_overlap-cross_height/2]) cube([knob_connector_cross_width, knob_connector_cross_span, cross_height], center=true);
                translate([0,0,knob_connector_length+cut_overlap-cross_height/2]) cube([knob_connector_cross_span, knob_connector_cross_width, cross_height], center=true);
                cylinder(d=knob_connector_core_diameter, h=knob_connector_length+cut_overlap);
            }
        }
    }

    pcb_outline_svg_offset_x = 0.3;
    pcb_outline_svg_offset_y = -0.8;

    // wall of the sleeve around the knurl bore
    side_thickness = 0.6;

    // printed shaft dimensions
    adapter_sleeve_diameter = knurl_bore + 2*side_thickness;
    adapter_knurl_depth = pec11s_shaft_length_above_body - pec11s_bushing_length - adapter_encoder_gap;
    adapter_key_height = pec11s_slot_depth - adapter_axial_clearance;
    adapter_sleeve_height = adapter_knurl_depth; // sleeve ends at the knurl tip, keeps it short inside the thread
    adapter_sleeve_chamfer_height = (adapter_sleeve_diameter - printed_shaft_diameter)/2;          // 45 degrees
    knob_connector_chamfer_height = (printed_shaft_diameter - knob_connector_core_diameter)/2;     // 45 degrees

    if (make_shaft) {
        if (shaft_print_orientation)
            color([1,0,0]) translate([0,0,adapter_total_length]) rotate([180,0,0]) SocketAdapter(side_thickness, 0);
        else
            color([1,0,0]) SocketAdapter(side_thickness, adapter_z);
    }

    if (render_encoder) {
        color([0.3,0.3,0.3]) translate([0,0,encoder_bottom_z]) {
            translate([0,0,pec11s_body_height/2]) cube([pec11s_body_width, pec11s_body_width, pec11s_body_height], center=true);
            cylinder(d=pec11s_bushing_diameter, h=pec11s_body_height+pec11s_bushing_length);
            cylinder(d=pec11s_neck_diameter, h=pec11s_body_height+pec11s_shaft_length_above_body);
            translate([0,0,pec11s_body_height+pec11s_shaft_length_above_body-pec11s_knurl_length]) difference() {
                cylinder(d=pec11s_knurl_diameter, h=pec11s_knurl_length);
                translate([0,0,pec11s_knurl_length-pec11s_slot_depth+pec11s_slot_depth/2]) cube([pec11s_knurl_diameter+1, pec11s_slot_width, pec11s_slot_depth+cut_overlap], center=true);
            }
        }
    }


    // base plate parameters
    base_plate_thickness = 1.6;
    base_plate_width_and_depth = 71.0;
    base_plate_corner_radius = 6.0;



    //shell_height = shell_bottom_thickness + space_at_bottom_of_shell + height_of_pcb_components_bottom + bottom_pcb_thickness + height_of_pcb_components_middle + top_pcb_thickness + space_between_top_pcb_and_base_plate;

    offset_base_plate = 0;//shell_height - shell_bottom_thickness - base_plate_thickness;


    reinforcement_ring_diameter = 14;
    reinforcement_ring_radius = reinforcement_ring_diameter / 2;
    reinforcement_ring_height = 1.2;

    base_plate_offset_z = -7;//13.6;


    //color([1,0,0]) translate([0,0,5]) cylinder(h=20, r=2.9, center=true);

    // shaft adapter

    thread_pitch = 1; // M10x1 (fine), thicker core than M10x1.5 for the bores inside
    thread_diameter = 10;
    thread_fit = -0.2;      // on the diameter of the printed outer thread, negative = looser fit in the nut

    // lock nut M10x1 DIN 13-5 (zinc plated steel)
    nut_width_across_flats = 12;
    nut_height = 3;
    
    height_thread_connector_including_base_plate = 4.0;
    
    // connectors on base plate whern shell can be fixed with crews
    shell_connector_height = 4.6;
    shell_connector_width = 3.0;
    shell_connector_depth = 16.0;
    shell_side_screw_diameter = 1.5;            // core hole in the base plate connectors for 2.0 mm self-tapping screws
    shell_side_screw_clearance_diameter = 2.2;  // through hole in the bottom shell, so the screw pulls the shell tight
    pcb_screw_core_diameter = 1.7;              // core hole in the PCB pillars for M2 screws
    
    button_hole_diameter = 3.0;
    
     // cutout rectangle for USB, needs to be recessed a bit since USB is not tall enough
    switch_cutout_width = 3.16 + 0.3;
    switch_cutout_depth = 8.94 + 0.3;
    switch_cutout_offset_x = 12.95 - switch_cutout_width/2;
    switch_cutout_offset_y = -17.2 - switch_cutout_depth/2;
    usb_cutout_recess_padding = 2.0;
    usb_cutout_recess_depth = 3.0;
    
    // Caps to cover 230V pins on front side   
    space_between_plate_and_pcb = 11.0; 
    connector_pin_offset_x = 0.75;
    connector_pin_offset_y = -19.95+0.76;
    connector_pin_offset_z = -space_between_plate_and_pcb+1;
    connector_pin_radius = 1;
    connector_pin_height = 2.5;
    
                
    clearance_moving_part = 0.1;
    printed_shaft_clearance = 0.15; // radial, printed shaft in the printed bore of the thread (FDM holes come out undersized)
    adapter_sleeve_clearance = 0.3;  // radial, sleeve of the printed shaft in the thread part (only guided by the shaft bore above)
    clearance_fixed_part = 0.04;
    connector_radius = 7.1; // outer radius of the thread part where it sits in the base plate (fixed, base plate must keep fitting)
    
    
    // rect that holds adapter behind base plate
    height_holder_rect = 6.0-base_plate_thickness;
    width_holder_rect_x = 2 * connector_radius+2.0;
    width_holder_rect_y = width_holder_rect_x;

    // axial layout of the thread part (z relative to base_plate_offset_z)
    reinforcement_ring_center_z = base_plate_thickness+reinforcement_ring_height/2-base_plate_thickness/2-0.5;
    reinforcement_ring_total_height = reinforcement_ring_height+base_plate_thickness+1;
    reinforcement_ring_top_z = reinforcement_ring_center_z + reinforcement_ring_total_height/2;
    thread_rod_bottom_z = -2.4; // start of the thread inside the reinforcement ring
    thread_top_z = base_plate_thickness + thread_length_above_base_plate;
    thread_rod_length = thread_top_z - thread_rod_bottom_z;
    thread_rod_center_z = (thread_rod_bottom_z + thread_top_z)/2;

    // engraved label on the side of the rect that holds the thread part behind the base plate
    thread_part_label_size = 2.5;
    thread_part_label_depth = 0.4;
    thread_part_bottom_z = -height_holder_rect;
    thread_minor_diameter = thread_diameter + thread_fit - 1.226869*thread_pitch; // ISO external thread core, 8.77 for M10x1 without fit

    // printed shaft position in the assembly (absolute z), sitting on the encoder shaft
    pcb_top_z = base_plate_offset_z + connector_pin_offset_z;
    encoder_bottom_z = pcb_top_z + pec11s_z_offset;
    encoder_bushing_top_z = encoder_bottom_z + pec11s_body_height + pec11s_bushing_length;
    adapter_z = encoder_bushing_top_z + adapter_encoder_gap;
    adapter_sleeve_top_z = adapter_z + adapter_sleeve_height - base_plate_offset_z; // relative to thread part
    adapter_shaft_round_length = (base_plate_offset_z + thread_top_z + printed_shaft_tip_above_thread - knob_connector_length) - (adapter_z + adapter_sleeve_height);
    adapter_total_length = adapter_sleeve_height + adapter_shaft_round_length + knob_connector_length;

    // bores in the thread part for the rotating shaft: wide for the sleeve, narrow for the round shaft
    adapter_sleeve_bore_diameter = adapter_sleeve_diameter + 2*adapter_sleeve_clearance;
    adapter_sleeve_bore_top_z = adapter_sleeve_top_z + adapter_axial_clearance + adapter_sleeve_bore_extra_height;
    printed_shaft_bore_diameter = printed_shaft_diameter + 2*printed_shaft_clearance;

    // hole for the fixed encoder bushing and the sleeve, from the PCB up to the thread part
    encoder_body_bore_diameter = max(pec11s_bushing_diameter + 2*encoder_bushing_clearance, adapter_sleeve_bore_diameter);
    encoder_body_bore_top_z = min(reinforcement_ring_top_z, adapter_sleeve_bore_top_z); // wide bore only where the wall is thick
    encoder_body_bore_bottom_z = connector_pin_offset_z; // PCB top

    // wall thickness checks
    sleeve_bore_wall = (adapter_sleeve_bore_top_z > reinforcement_ring_top_z ? thread_minor_diameter/2 : connector_radius) - adapter_sleeve_bore_diameter/2;
    shaft_bore_wall = (thread_minor_diameter - printed_shaft_bore_diameter)/2;
    echo(str("Printed shaft: sleeve d=", adapter_sleeve_diameter, " h=", adapter_sleeve_height, ", knurl bore ", knurl_bore, " x ", adapter_knurl_depth,
             " deep, key ", adapter_key_width, " x ", adapter_key_height, ", shaft d=", printed_shaft_diameter, ", total length ", adapter_total_length, " mm"));
    echo(str("Sleeve bore: d=", adapter_sleeve_bore_diameter, " up to z=", adapter_sleeve_bore_top_z, ", wall to thread core (", thread_minor_diameter, "): ", sleeve_bore_wall, " mm"));
    echo(str("Shaft bore: d=", printed_shaft_bore_diameter, " up to z=", thread_top_z, ", wall to thread core ", shaft_bore_wall, " mm"));
    echo(str("Encoder body bore: d=", encoder_body_bore_diameter, " up to z=", encoder_body_bore_top_z, ", fixed encoder bushing reaches ", encoder_bushing_top_z - base_plate_offset_z - thread_part_bottom_z, " mm into the thread part"));
    assert(adapter_knurl_depth > 0, "encoder shaft too short for the printed shaft");
    assert(adapter_sleeve_bore_top_z < thread_top_z, "sleeve reaches beyond the end of the thread");
    assert(adapter_shaft_round_length >= adapter_web_thickness, "no room for the closed top of the knurl bore");
    assert(adapter_shaft_round_length >= adapter_sleeve_chamfer_height + knob_connector_chamfer_height, "printed shaft too short for its chamfers");
    
    if(make_base_plate_thread ) {
    
   
    // Make inner thread adapter to be inserted from back of plate (so we can print both parts better)
    
    difference() {
     
    color([1,0,0]) translate([0,0,base_plate_offset_z]) difference() {
        union() {              
        // reinforcement ring around bushing
        translate([0,0,reinforcement_ring_center_z]) cylinder(h=reinforcement_ring_total_height, r=connector_radius, center=true);
        
        // thread / bushing                
        translate([0,0,thread_rod_center_z]) threaded_rod(d=thread_diameter+thread_fit, l=thread_rod_length, pitch=thread_pitch, left_handed=false, blunt_start2=true, blunt_start1=false, $slop=0.1);
        
        // reinforcement ring around encoder
        
        //translate([0,0,-2.6]) cylinder(h=4.2, r=connector_radius);
        
        // rect that holds adapter behind base plate
        translate([-width_holder_rect_x/2,-width_holder_rect_y/2,-height_holder_rect]) linear_extrude(height_holder_rect) square([width_holder_rect_x,width_holder_rect_y]);
    }  


    // bores for the rotating printed shaft
    translate([0,0,thread_part_bottom_z-cut_overlap]) cylinder(h=encoder_body_bore_top_z-thread_part_bottom_z+cut_overlap, d=encoder_body_bore_diameter);
    translate([0,0,encoder_body_bore_top_z-cut_overlap]) cylinder(h=(encoder_body_bore_diameter-adapter_sleeve_bore_diameter)/2+cut_overlap, d1=encoder_body_bore_diameter, d2=adapter_sleeve_bore_diameter); // 45 degrees, no flat overhang
    translate([0,0,encoder_body_bore_top_z-cut_overlap]) cylinder(h=adapter_sleeve_bore_top_z-encoder_body_bore_top_z+cut_overlap, d=adapter_sleeve_bore_diameter);
    translate([0,0,adapter_sleeve_bore_top_z-cut_overlap]) cylinder(h=(adapter_sleeve_bore_diameter-printed_shaft_bore_diameter)/2+cut_overlap, d1=adapter_sleeve_bore_diameter, d2=printed_shaft_bore_diameter); // 45 degrees, matches the shaft chamfer
    translate([0,0,adapter_sleeve_bore_top_z-cut_overlap]) cylinder(h=thread_top_z-adapter_sleeve_bore_top_z+2*cut_overlap, d=printed_shaft_bore_diameter);

    // hole for encoder body and sleeve
    translate([0,0,encoder_body_bore_bottom_z]) cylinder(h=thread_part_bottom_z-encoder_body_bore_bottom_z, d=encoder_body_bore_diameter);

    // label on the side
    if (thread_part_label != "")
        translate([0,-width_holder_rect_y/2+thread_part_label_depth,-height_holder_rect/2]) rotate([90,0,0]) linear_extrude(thread_part_label_depth+cut_overlap)
            text(thread_part_label, size=thread_part_label_size, halign="center", valign="center");

            
    }
        // TODO uncomment for cross section
        //translate([-400,0,-400]) linear_extrude(h=800) square([800,400]);
    }
    }
    
    if(make_nut) {
        translate([0,0,5]) threaded_nut(nutwidth=nut_width_across_flats, id=thread_diameter, h=nut_height, pitch=thread_pitch, left_handed=false, blunt_start=false, bevel=false, $slop=0.1);
    }
    if(make_base_plate) {
    
    
    // make base plate
    translate([0,0,base_plate_offset_z]) difference() {
            union() {
                // base plate with cutouts and hole for switch, LED, etc.
                base_plate(base_plate_thickness, base_plate_width_and_depth, base_plate_corner_radius, offset_base_plate);
                
                difference() {
                extra_width_holder_rect_holder_x = 27.0;
                extra_width_holder_rect_holder_y = 6.0;
                width_holder_rect_holder_x = width_holder_rect_x + extra_width_holder_rect_holder_x;
                width_holder_rect_holder_y = width_holder_rect_y + extra_width_holder_rect_holder_y;
                    
        translate([-width_holder_rect_holder_x/2,-width_holder_rect_holder_y/2,-height_holder_rect]) linear_extrude(height_holder_rect) square([width_holder_rect_holder_x,width_holder_rect_holder_y]);
        
        translate([-(width_holder_rect_x+clearance_fixed_part)/2,-(width_holder_rect_y+clearance_fixed_part)/2,-height_holder_rect-1]) linear_extrude(height_holder_rect+2) square([width_holder_rect_x+clearance_fixed_part*2,width_holder_rect_y+clearance_fixed_part*2]);
                }
          
                
                // connectors on base plate where shell can be fixed with screws                
                translate([20.98,0,-shell_connector_height]) linear_extrude(shell_connector_height+base_plate_thickness) square([shell_connector_width,shell_connector_depth], center=true);
                translate([-20.38,0,-shell_connector_height]) linear_extrude(shell_connector_height+base_plate_thickness) square([shell_connector_width,shell_connector_depth], center=true);
                
                // inside thread
                //translate([0,0,-0.4]) threaded_nut(nutwidth=16, id=thread_diameter, h=height_thread_connector_including_base_plate, pitch=thread_pitch, shape="square", bevel=false, blunt_start=false, $slop=0.05, $fa=1, $fs=1);   


                // Make caps to cover 230V pins on front side
                
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
                        union() {                        
                            // make bulky base so that screws don't break the sockets, then narrow cylinders only where it touches the board
                            // need to cut bulky part with board outline, since it extends past that
                            intersection() {
                                translate([pcb_outline_svg_offset_x,pcb_outline_svg_offset_y,-20]) linear_extrude(bottom_shell_height) import(file="esp_edge_cuts_v2.svg",center=true);
                            
                                space_between_bukly_part_and_board_1 = 1.0; // height of tallest component in the way
                                color([1,0,1]) translate([18.56,-10.52,connector_pin_offset_z+space_between_bukly_part_and_board_1]) cylinder(h=space_between_plate_and_pcb-space_between_bukly_part_and_board_1,r=5);
                            }
                            translate([18.56,-10.52,connector_pin_offset_z]) cylinder(h=space_between_plate_and_pcb,r=2);
                        }
                        translate([18.56,-10.52,connector_pin_offset_z-1]) cylinder(h=10,d=pcb_screw_core_diameter);
                    }
                    
                    
                    color([1,0,0]) difference() {
                        union() {
                            intersection() {
                                translate([pcb_outline_svg_offset_x,pcb_outline_svg_offset_y,-20]) linear_extrude(bottom_shell_height) import(file="esp_edge_cuts_v2.svg",center=true);
                            space_between_bukly_part_and_board_2 = 2.0; // height of tallest component in the way
                                translate([-16.65,16.54,connector_pin_offset_z+space_between_bukly_part_and_board_2]) cylinder(h=space_between_plate_and_pcb-space_between_bukly_part_and_board_2,r=5);
                            }
                            translate([-16.65,16.54,connector_pin_offset_z]) cylinder(h=space_between_plate_and_pcb,r=2);
                        }
                        translate([-16.65,16.54,connector_pin_offset_z-1]) cylinder(h=10,d=pcb_screw_core_diameter);
                    }
                    
                    
                }
                
                // USB cutout            
                // make thicker for USB                              
                translate([switch_cutout_offset_x-usb_cutout_recess_padding-1.0, switch_cutout_offset_y-usb_cutout_recess_padding-1.0, -base_plate_thickness]) linear_extrude(base_plate_thickness+2, center=true) make_rounded_rectangle(switch_cutout_width+usb_cutout_recess_padding*2+2, switch_cutout_depth+usb_cutout_recess_padding*2+2, 1);
                
            }  
            
            // hole for adapter in center
            translate([0,0,10]) cylinder(h=60, r=connector_radius+clearance_fixed_part, center=true);
          
            // recess for USB
            translate([switch_cutout_offset_x-usb_cutout_recess_padding, switch_cutout_offset_y-usb_cutout_recess_padding, base_plate_thickness+2-usb_cutout_recess_depth]) linear_extrude(base_plate_thickness+2, center=true) make_rounded_rectangle(switch_cutout_width+usb_cutout_recess_padding*2, switch_cutout_depth+usb_cutout_recess_padding*2, 2);
            // hole for USB
            translate([switch_cutout_offset_x, switch_cutout_offset_y, 0]) linear_extrude(base_plate_thickness+40, center=true) make_rounded_rectangle(switch_cutout_width, switch_cutout_depth, 1); 
            
            
            // subtract cutout for buttons          
            translate([-12.3,-1.4,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
            translate([-4.1,-11.0,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
            translate([14.3,-3.4,0]) cylinder(h=10, r=button_hole_diameter/2, center=true);
            
            // holes for screws in side connectors
            translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-60]) cylinder(r=shell_side_screw_diameter/2, h=120);
        }
        
        
        
    }
    
    
    
    
    
    /*
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
            translate([0,0,10]) cylinder(h=60, r=(shaft_radius-0.5+0.2+0.5), center=true);
            
            // hole for knob wrapper
            translate([0,0,-3.4]) cylinder(h=11, r=shaft_radius+0.4);
            
            
            // hole for encoder body
            translate([0,0,-3.4]) cylinder(h=5, r=shaft_radius+0.6);
        }
    
    }
    */
    
    bottom_shell_height = 29;
    bottom_shell_thickness = 1;
    bottom_shell_bottom_z = -bottom_shell_height-7;

    // labels for the power terminal on the outside of the bottom shell floor, inlaid flush so they can be printed in a
    // different color (two STLs, loaded in Bambu Studio as one object with two parts)
    power_connector_hole_pitch = 5.0;
    power_connector_labels = ["L'", "L", "N"]; // left to right seen from the top: relay output, live, neutral
    power_connector_label_size = 3.0;
    power_connector_label_depth = 0.4;          // two layers at 0.2, floor is bottom_shell_thickness
    power_connector_label_y = connector_pin_offset_y + 4.7; // above the holes, between them and the board center
    power_connector_label_font = "Liberation Sans:style=Bold";

    module power_connector_labels(extra_depth=0) {
        for (i=[-1:1:1]) {
            translate([0.7 + i * power_connector_hole_pitch, power_connector_label_y, bottom_shell_bottom_z-extra_depth])
                linear_extrude(power_connector_label_depth+extra_depth)
                    mirror([1,0,0]) // read from below
                        text(power_connector_labels[i+1], size=power_connector_label_size, font=power_connector_label_font, halign="center", valign="center");
        }
    }
    
    if (build_bottom_shell) {
    
        translate([0,0,0]) difference() {
            translate([0,0.76, -bottom_shell_height-7]) difference() {
                translate([pcb_outline_svg_offset_x,pcb_outline_svg_offset_y,0]) linear_extrude(bottom_shell_height) offset(1) import(file="esp_edge_cuts_v2.svg",center=true);
                translate([pcb_outline_svg_offset_x,pcb_outline_svg_offset_y,bottom_shell_thickness]) linear_extrude(bottom_shell_height+1) offset(0.2) import(file="esp_edge_cuts_v2.svg",center=true);
            }
            
            // cut out for power connector
            power_connector_hole_width = 4.0;
            for (i=[-1:1:1]) {
                power_connector_x = 0.7 + i * power_connector_hole_pitch;
                translate([power_connector_x,-22,-26.0]) linear_extrude(6.0) square([power_connector_hole_width,10.0], center=true);
                translate([power_connector_x,connector_pin_offset_y,-90.0]) cylinder(r=power_connector_hole_width/2, h=100.0);
            }
        
            translate([0,0,base_plate_offset_z]) translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-62]) cylinder(d=shell_side_screw_clearance_diameter, h=120);
                     
            translate([0,0,base_plate_offset_z]) translate([0,0,-shell_connector_height/2]) rotate([0, 90, 0]) translate([0,0,-62]) cylinder(d=shell_side_screw_clearance_diameter, h=120);
            
            // hole for JST connectors
            translate([24.0,4.8,-17.4]) linear_extrude(4.0) square([10.0,8.0], center=true);
            translate([24.0,12.3,-17.4]) linear_extrude(4.0) square([10.0,5.0], center=true);
            
            // hole at top for USB connector (needs space outside shell)
            translate([switch_cutout_offset_x-usb_cutout_recess_padding-1.05, switch_cutout_offset_y-usb_cutout_recess_padding-1.05, base_plate_offset_z-base_plate_thickness]) linear_extrude(base_plate_thickness+2.05, center=true) make_rounded_rectangle(switch_cutout_width+usb_cutout_recess_padding*2+2.1, switch_cutout_depth+usb_cutout_recess_padding*2+2.1, 1);

            // pockets for the label inlays
            power_connector_labels(cut_overlap);
        }
        
    }

    if (make_bottom_shell_labels) {
        color([1,1,1]) power_connector_labels();
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
if (render_main_model) my_model();
//translate([0,0,-90]) cylinder(h=680, r=7);
//translate([-20,0,-90]) linear_extrude(180) square([40,80], center=true);
//}
