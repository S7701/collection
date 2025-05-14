// constants

len = 100.0;

tube_outer_dia = 15.0;
tube_wall_thk = 1.5;
tube_nozzle_dia = 7.0;

pipe_outer_dia = 5.0;
pipe_inner_dia = 3.0;
pipe_len = 20;

// calculations

function sqr(x) = x*x;

tube_outer_radius  = tube_outer_dia / 2;
tube_inner_radius  = tube_outer_radius - tube_wall_thk;
tube_inner_dia     = tube_inner_radius * 2;
tube_nozzle_radius = tube_nozzle_dia / 2;
tube_nozzle_len    = sqrt(sqr(tube_outer_radius) - sqr(tube_nozzle_radius));

pipe_outer_radius = pipe_outer_dia / 2;
pipe_inner_radius = pipe_inner_dia / 2;
pipe_tip_len      = sqrt(sqr(pipe_outer_radius) - sqr(pipe_inner_radius));

$fn = $preview ? 60 : 120;

eps = $preview ? 0.1 : 0.01;

module tube() {
    translate([0, 0, pipe_tip_len + tube_nozzle_len]) {
        difference() {
            union() {
                sphere(d = tube_outer_dia);
                cylinder(h = len - tube_nozzle_len - pipe_tip_len, d = tube_outer_dia);
            }
            // subtract
            translate([0, 0, -(tube_outer_radius + eps)]) cylinder(h = tube_outer_radius + eps, d = tube_nozzle_dia);
            sphere(d = tube_inner_dia);
            cylinder(h = len - tube_nozzle_len - pipe_tip_len + eps, d = tube_inner_dia);

            if ($preview) translate([-(tube_outer_radius + eps), -(tube_outer_radius + eps), -(tube_outer_radius + eps)]) cube([tube_outer_dia + eps * 2, tube_outer_radius + eps, len + eps * 2]);
        }
    }
}

module pipe() {
    difference() {
        translate([0, 0, pipe_outer_radius]) {
            sphere(d=pipe_outer_dia);
            cylinder(h=len-pipe_outer_radius, d=pipe_outer_dia);
            translate([0, 0, len-pipe_outer_radius]) {
                sphere(d=pipe_outer_dia);
            }
        }
        // subtract
        cylinder(h=len+2*eps, d=pipe_inner_dia);
        translate([0, 0, len]) {
            sphere(d=pipe_inner_dia);
        }

        if ($preview) translate([-pipe_outer_radius-eps, -pipe_outer_radius-eps, -eps]) cube([pipe_outer_dia+2*eps, pipe_outer_radius+eps, len+2*eps+pipe_outer_radius]);
    }
}

tube();
pipe();
