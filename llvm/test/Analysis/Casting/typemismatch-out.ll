; REQUIRES: asserts
; RUN: opt -stats -debug-only=ArgUsage -legacy-fnargusage -disable-output %s 2>&1 | FileCheck %s --check-prefixes=CHECK,NO
; RUN: opt -stats -debug-only=ArgUsage -legacy-fnargusage -disable-output %s 2>&1 | FileCheck %s --check-prefixes=CHECK,CAST

; CHECK-LABEL: callee
; CHECK: takes 2 parameters:
; CHECK-NEXT: a: i32
; CHECK-NEXT: b: i32
define dso_local i32 @callee(i32 %a, i32 %b) {
    %add = add nsw i32 %a, %b
    ret i32 %add
}

; NO-LABEL: caller1
; NO: arg #0
; NO-NOT: {{.+}}
; NO-SAME: : i32
; NO-NOT: type mismatch:
; NO-NEXT: arg #1
; NO-NOT: {{.+}}
; NO-SAME: : i32
; NO-NOT: type mismatch:
define dso_local i32 @caller1() {
  %1 = call i32 @callee(i32 97, i32 98)
  ret i32 %1
}

; CAST-LABEL: caller2
; CAST: arg #0 (arg2): i8
; CAST-NEXT: type mismatch: expected 'i32' but argument is of type 'i8'
; CAST-NEXT: arg #1 (arg1): double
; CAST-NEXT: type mismatch: expected 'i32' but argument is of type 'double'
; CAST: Release memory

; CAST-LABEL: caller2
; CAST:  takes 2 parameters:
; CAST-NEXT: op1: i8
; CAST-NEXT: op2: double
; CAST-NEXT: Release memory
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

; CHECK: 2 ArgUsage - Number of type mismatches are found
