; RUN: opt -legacy-fnargusage -analyze %s | FileCheck %s

; CHECK: Printing analysis 'Function Argument Usage Pass' for function 'callee':
; CHECK-NEXT: Function 'caller' call on line '4': argument type mismatch. Argument #0 Expected 'i32' but argument is of type 'i8'
; CHECK-NEXT: Function 'caller' call on line '4': argument type mismatch. Argument #1 Expected 'i32' but argument is of type 'double'
define dso_local i32 @callee(i32 %a, i32 %b) !dbg !10 {
entry:
  %b.addr = alloca i32, align 4
  %a.addr = alloca i32, align 4
  store i32 %b, i32* %b.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %b.addr, metadata !15, metadata !DIExpression()), !dbg !16
  store i32 %a, i32* %a.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %a.addr, metadata !17, metadata !DIExpression()), !dbg !16
  %0 = load i32, i32* %a.addr, align 4, !dbg !18
  %1 = load i32, i32* %b.addr, align 4, !dbg !18
  %add = add i32 %0, %1, !dbg !18
  ret i32 %add, !dbg !18
}

declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

define dso_local i32 @caller() !dbg !19 {
entry:
  %call = call i32 bitcast (i32 (i32, i32)* @callee to i32 (i8, double)*)(i8 97, double 1.010000e+01), !dbg !22
  ret i32 %call, !dbg !22
}

!llvm.dbg.cu = !{!0, !3}
!llvm.ident = !{!5, !5}
!llvm.module.flags = !{!6, !7, !8, !9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 9.0.0 (https://github.com/llvm/llvm-project.git 2121a4f7335a9e4985997d4d880c11c588b48a27)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "typemismatch1.c", directory: "C:\5CWork\5CDev\5Cllvm\5Cllvm-my-test-vectorization", checksumkind: CSK_MD5, checksum: "5ef510dc9101782321d459b8c52a6860")
!2 = !{}
!3 = distinct !DICompileUnit(language: DW_LANG_C99, file: !4, producer: "clang version 9.0.0 (https://github.com/llvm/llvm-project.git 2121a4f7335a9e4985997d4d880c11c588b48a27)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!4 = !DIFile(filename: "typemismatch3.c", directory: "C:\5CWork\5CDev\5Cllvm\5Cllvm-my-test-vectorization", checksumkind: CSK_MD5, checksum: "99b730e1bca82d9489c27487e965027f")
!5 = !{!"clang version 9.0.0 (https://github.com/llvm/llvm-project.git 2121a4f7335a9e4985997d4d880c11c588b48a27)"}
!6 = !{i32 2, !"CodeView", i32 1}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 2}
!9 = !{i32 7, !"PIC Level", i32 2}
!10 = distinct !DISubprogram(name: "callee", scope: !1, file: !1, line: 1, type: !11, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !14, !14}
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!15 = !DILocalVariable(name: "b", arg: 2, scope: !10, file: !1, line: 1, type: !14)
!16 = !DILocation(line: 1, scope: !10)
!17 = !DILocalVariable(name: "a", arg: 1, scope: !10, file: !1, line: 1, type: !14)
!18 = !DILocation(line: 3, scope: !10)
!19 = distinct !DISubprogram(name: "caller", scope: !4, file: !4, line: 3, type: !20, scopeLine: 3, spFlags: DISPFlagDefinition, unit: !3, retainedNodes: !2)
!20 = !DISubroutineType(types: !21)
!21 = !{!13}
!22 = !DILocation(line: 4, scope: !19)
