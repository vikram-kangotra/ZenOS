(module
  ;; Function that adds two i32 numbers
  (func $add (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)

  ;; Function that multiplies two i32 numbers
  (func $mul (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.mul) 

  ;; Export the functions
  (export "add" (func $add))
  (export "mul" (func $mul)))