if exists("b:current_syntax")
  finish
endif

syn keyword nicklKeyword dreak cast continue do else fn for if in return struct while defer type_t array_t ptr_t tuple_t foreign
syn keyword nicklType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 int float void
syn keyword nicklBoolean true false

syn keyword nicklStruct struct nextgroup=nicklStructName skipwhite skipempty
syn match nicklStructName display '\w\+' contained

syn match nicklComment    display '//.*$'
syn match nicklFunction   display '\<\w\+[(]'me=e-1
syn match nicklSymbol     display '#\w\+'
syn match nicklStructName display '\<\w\+\s*{'me=e-1
syn match nicklNumber     display '\(\.\d\+\|\<\d\+\.\=\d*\)\([eE][-+]\=\d\+\>\)\='

" TODO Escape sequences

syn region nicklString start='"' end='"'

let b:current_syntax = "nickl"

hi def link nicklKeyword    Keyword
hi def link nicklNumber     Number
hi def link nicklString     String
hi def link nicklComment    Comment
hi def link nicklFunction   Function
hi def link nicklStruct     Structure
hi def link nicklSymbol     PreProc
hi def link nicklType       Type
hi def link nicklBoolean    Boolean
hi def link nicklStructName Function
