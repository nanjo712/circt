// RUN: circt-opt --sim-lower-print-formatted-proc-to-sv %s | FileCheck %s

// This pass assumes sim.proc.print is already in an SV procedural root.
// It intentionally does not lower sim.proc.print under hw.triggered.

// CHECK-LABEL: hw.module @proc_print
hw.module @proc_print(in %arg: i8) {
  // CHECK: sv.initial {
  sv.initial {
    %l0 = sim.fmt.literal "err: "
    %f0 = sim.fmt.hex %arg, isUpper true specifierWidth 2 : i8
    %l1 = sim.fmt.literal " 100%"
    %msg = sim.fmt.concat (%l0, %f0, %l1)

    // CHECK-NEXT: %[[FD:.+]] = hw.constant -2147483646 : i32
    // CHECK-NEXT: sv.fwrite %[[FD]], "err: %02X 100%%"(%arg) : i8
    // CHECK-NOT: sim.fmt.
    sim.proc.print %msg
  }
}


// CHECK-LABEL: hw.module @all_format_fragments
hw.module @all_format_fragments(in %ival : i16, in %ch : i8, in %fval : f64) {
  sv.initial {
    %i0 = sim.fmt.literal "dec="
    %f0 = sim.fmt.dec %ival specifierWidth 6 signed : i16
    %i1 = sim.fmt.literal " hex="
    %f1 = sim.fmt.hex %ival, isUpper true paddingChar 48 specifierWidth 4 : i16
    %i2 = sim.fmt.literal " oct="
    %f2 = sim.fmt.oct %ival isLeftAligned true specifierWidth 6 : i16
    %i3 = sim.fmt.literal " bin="
    %f3 = sim.fmt.bin %ival paddingChar 32 specifierWidth 8 : i16
    %i4 = sim.fmt.literal " char="
    %f4 = sim.fmt.char %ch : i8
    %i5 = sim.fmt.literal " exp="
    %f5 = sim.fmt.exp %fval fieldWidth 10 fracDigits 3 : f64
    %i6 = sim.fmt.literal " flt="
    %f6 = sim.fmt.flt %fval isLeftAligned true fieldWidth 8 fracDigits 2 : f64
    %i7 = sim.fmt.literal " gen="
    %f7 = sim.fmt.gen %fval fracDigits 4 : f64
    %i8 = sim.fmt.literal " path="
    %f8 = sim.fmt.hier_path
    %i9 = sim.fmt.literal " esc="
    %f9 = sim.fmt.hier_path escaped
    %i10 = sim.fmt.literal " pct=%"
    %msg = sim.fmt.concat (%i0, %f0, %i1, %f1, %i2, %f2, %i3, %f3, %i4, %f4, %i5, %f5, %i6, %f6, %i7, %f7, %i8, %f8, %i9, %f9, %i10)

    // CHECK: %[[FD:.+]] = hw.constant -2147483646 : i32
    // CHECK-NEXT: sv.fwrite %[[FD]], "dec=%6d hex=%04X oct=%-06o bin=%8b char=%c exp=%10.3e flt=%-8.2f gen=%.4g path=%m esc=%M pct=%%"(%ival, %ival, %ival, %ival, %ch, %fval, %fval, %fval) : i16, i16, i16, i16, i8, f64, f64, f64
    // CHECK-NOT: sim.fmt.
    sim.proc.print %msg
  }
}

// CHECK-LABEL: hw.module @nested_concat_order
hw.module @nested_concat_order(in %lhs : i8, in %rhs : i8) {
  sv.initial {
    %l0 = sim.fmt.literal "L="
    %l1 = sim.fmt.literal ", R="
    %d0 = sim.fmt.dec %lhs specifierWidth 3 : i8
    %h0 = sim.fmt.hex %rhs, isUpper false specifierWidth 2 : i8

    %c0 = sim.fmt.concat (%l0, %d0)
    %c1 = sim.fmt.concat (%c0, %l1)
    %c2 = sim.fmt.concat (%c1, %h0)

    // CHECK: %[[FD:.+]] = hw.constant -2147483646 : i32
    // CHECK-NEXT: sv.fwrite %[[FD]], "L=%3d, R=%02x"(%lhs, %rhs) : i8, i8
    // CHECK-NOT: sim.fmt.
    sim.proc.print %c2
  }
}

// CHECK-LABEL: hw.module @dce_uses_outer_procedural_root
hw.module @dce_uses_outer_procedural_root(in %cond : i1, in %val : i8) {
  // CHECK: sv.initial {
  sv.initial {
    %lit = sim.fmt.literal "v="
    %fmt = sim.fmt.dec %val : i8
    %msg = sim.fmt.concat (%lit, %fmt)
    // CHECK: sv.if %cond {
    // CHECK: sv.fwrite %{{.*}}, "v=%d"(%val) : i8
    sv.if %cond {
      sim.proc.print %msg
    }
  }
  // CHECK-NOT: sim.fmt.
}
