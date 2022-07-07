if exists("b:current_syntax")
  finish
endif

syn keyword nicklKeyword break cast continue do else fn for if in return struct while defer foreign pub import here const interface union enum dyn use
syn keyword nicklType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 int float void bool string any_t type_t array_t ptr_t tuple_t usize isize
syn keyword nicklBoolean true false

syn keyword nicklStruct struct nextgroup=nicklStructName skipwhite skipempty
syn keyword nicklInterface interface nextgroup=nicklStructName skipwhite skipempty
syn keyword nicklUnion union nextgroup=nicklStructName skipwhite skipempty
syn keyword nicklEnum enum nextgroup=nicklStructName skipwhite skipempty
syn match nicklStructName display '\w\+' contained

syn match nicklComment    display '//.*$'
syn match nicklFunction   display '\<\w\+[(]'me=e-1
syn match nicklSymbol     display '#\w\+'
syn match nicklDollar     display '\$\w\+'
syn match nicklAt         display '@\w\+'
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
hi def link nicklInterface  Structure
hi def link nicklUnion      Structure
hi def link nicklEnum       Structure
hi def link nicklSymbol     PreProc
hi def link nicklDollar     PreProc
hi def link nicklAt         PreProc
hi def link nicklType       Type
hi def link nicklBoolean    Boolean
hi def link nicklStructName Function
