$fa = 1;
$fs = 0.2;

translate([0,0,9.06+7/2]) cylinder(h=12, r=2, center=true);
translate([0,0,2.06+7/2]) cylinder(h=7, r=3, center=true);
translate([0,0,2+0.6/2]) cylinder(h=0.6, r=3.5, center=true);
// subtract cutout for buttons
translate([0,0,0]) linear_extrude(height=2) square([3,2], center=true);
translate([0,0,-0.4/2]) cylinder(h=0.4, r=1, center=true);