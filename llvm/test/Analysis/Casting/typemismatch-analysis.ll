; RUN: opt -legacy-fnargusage -analyze %s | FileCheck %s

; CHECK: Printing analysis 'Function Argument Usage Pass' for function 'callee':
; CHECK-NEXT: Function 'caller2': argument type mismatch. Argument #0 Expected 'i32' but argument is of type 'i8'
; CHECK-NEXT: Function 'caller2': argument type mismatch. Argument #1 Expected 'i32' but argument is of type 'double'
define dso_local i32 @callee(i32 %a, i32 %b) {
    %add = add nsw i32 %a, %b
    ret i32 %add
}

define dso_local i32 @caller2(i8 %op1, double %op2) {
  %op2.addr = alloca double, align 8
  %op1.addr = alloca i8, align 8
  store double %op2, double* %op2.addr, align 8
  store i8 %op1, i8* %op1.addr, align 8
  %arg1 = load double, double* %op2.addr, align 8
  %arg2 = load i8, i8* %op1.addr, align 8
  %call = call i32 bitcast (i32 (i32, i32)* @callee to i32 (i8, double)*)(i8 %arg2, double %arg1)
  ret i32 %call
}
