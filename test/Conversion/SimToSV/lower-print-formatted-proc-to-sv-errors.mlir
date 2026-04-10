// RUN: circt-opt --sim-lower-print-formatted-proc-to-sv --split-input-file --verify-diagnostics %s

hw.module @unsupported_padding_char(in %arg : i8) {
  sv.initial {
    %lit = sim.fmt.literal "bad="
    %bad = sim.fmt.dec %arg paddingChar 42 specifierWidth 2 : i8
    %msg = sim.fmt.concat (%lit, %bad)
    // expected-error @below {{cannot lower 'sim.proc.print' to sv.fwrite: sim.fmt.dec only supports paddingChar 32 (' ') or 48 ('0') for SystemVerilog lowering}}
    sim.proc.print %msg
  }
}

// -----

hw.module @unsupported_input_block_argument(in %arg : !sim.fstring) {
  sv.initial {
    // expected-error @below {{cannot lower 'sim.proc.print' to sv.fwrite: block argument format strings are unsupported as sim.proc.print input}}
    sim.proc.print %arg
  }
}

// -----

hw.module @unsupported_procedural_root(in %clk : i1) {
  hw.triggered posedge %clk {
    %lit = sim.fmt.literal "hello"
    // expected-error @below {{must be contained in a supported SV procedural root (sv.initial/sv.always/sv.alwayscomb/sv.alwaysff) before running --sim-lower-print-formatted-proc-to-sv}}
    sim.proc.print %lit
  }
}
