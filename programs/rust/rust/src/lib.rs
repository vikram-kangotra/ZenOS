unsafe extern "C" {
    fn print(_: i32);
}

fn fact(a: i32) -> i32 {
    if a == 0 || a == 1 { return 1; }
    a * fact(a - 1)
}

#[unsafe(no_mangle)]
pub extern "C" fn main() {
    for i in 1..10 {
        unsafe { print(fact(i)); }
    }
}
