; ModuleID = 'minic_module'
source_filename = "minic_module"
target triple = "x86_64-pc-linux-gnu"

declare void @print(i32)

declare i32 @read()

define i32 @func(i32 %0) {
entry:
  %"b$2" = alloca i32, align 4
  %"a$1" = alloca i32, align 4
  %"p$0" = alloca i32, align 4
  %ret = alloca i32, align 4
  store i32 %0, ptr %"p$0", align 4
  store i32 10, ptr %"a$1", align 4
  %loadtmp = load i32, ptr %"a$1", align 4
  %loadtmp1 = load i32, ptr %"p$0", align 4
  %addtmp = add i32 %loadtmp, %loadtmp1
  store i32 %addtmp, ptr %"b$2", align 4
  %loadtmp2 = load i32, ptr %"b$2", align 4
  store i32 %loadtmp2, ptr %ret, align 4
  br label %return

return:                                           ; preds = %entry
  %retload = load i32, ptr %ret, align 4
  ret i32 %retload
}
