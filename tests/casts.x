extern fn printf(fmt: str, ...) -> i32;

fn main() -> i32 {
    let fmt_int: str = "%s: %d\n";
    let fmt_flt: str = "%s: %f\n";

    let val_i64: i64 = 300;
    let val_i8: i8 = val_i64 as i8;
    printf(fmt_int, "Truncate (300 as i8) [Expect 44]", val_i8 as i32);

    let val_neg: i8 = -15;
    let val_sext: i64 = val_neg as i64;
    printf(fmt_int, "Sign Extend (-15 as i64) [Expect -15]", val_sext as i32);

    let val_u8: u8 = 200;
    let val_zext: u64 = val_u8 as u64;
    printf(fmt_int, "Zero Extend (200 as u64) [Expect 200]", val_zext as i32);

    let val_i32: i32 = 42;
    let val_f32: f32 = val_i32 as f32;
    let val_f64: f64 = val_f32 as f64;
    printf(fmt_flt, "Int -> f32 -> f64 [Expect 42.000000]", val_f64);

    let val_pi: f64 = 3.14159;
    let val_pi_int: i32 = val_pi as i32;
    printf(fmt_int, "Double -> Int (3.14159 as i32) [Expect 3]", val_pi_int);

    let d_large: f64 = 123.456;
    let f_small: f32 = d_large as f32;
    let d_back: f64 = f_small as f64;
    printf(fmt_flt, "Double -> f32 -> f64 [Expect ~123.456001]", d_back);

    let u_large: u32 = 4000000000;
    let f_large: f64 = u_large as f64;
    printf(fmt_flt, "Unsigned 32-bit -> f64 [Expect 4000000000.000000]", f_large);

    ret 0;
}
