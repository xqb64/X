extern fn __x_sleep_ms(ms: i64) -> i64;
extern fn printf(fmt: str, ...) -> i32;
extern fn malloc(n: u64) -> *void;
extern fn free(ptr: *void) -> void;

let NUM_TASKS: u64 = 5;

async fn worker(n: u64) -> u64 {
  loop {
    printf("hello from worker %lld\n", n);
    __x_sleep_ms(1000);
  }
  ret 0;
}

fn main(void) -> i32 {
  let mut x: i32 = 0;
  let mut y: i32 = 0;
  let mut arr: *task = malloc(NUM_TASKS * sizeofT(task));
  while (x < NUM_TASKS) {
    let t: task = worker(x);
    *(arr+x) = t;
    x = x+1;
  }
  while (y < NUM_TASKS) {
    await *(arr+y);
    y = y+1;
  }
  ret 0;
}
