if exists("b:current_syntax")
  finish
endif

syn keyword nicklKeyword break cast continue do else fn for if in return struct while defer foreign pub import here const interface union enum dyn use impl match mut null proc
syn keyword nicklType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 int float void bool string any_t type_t array_t ptr_t tuple_t usize isize
syn keyword nicklBoolean true false

syn match nicklComment    display '//.*$'
syn match nicklFunction   display '\<\w\+[(]'me=e-1
syn match nicklSymbol     display '#\w\+'
syn match nicklDollar     display '\$\w\+'
syn match nicklAt         display '@\w\+'
syn match nicklComptime   display '\<\w\+\s*::'me=e-2
syn match nicklNumber     display '\(0x[0-9a-fA-F_]\+\|\.[0-9_]\+\|\<[0-9_]\+\.\=\d*\)\([eE][-+]\=\d\+\>\)\='

syn region nicklComment   start="/\*"    end="\*/"

" TODO Escape sequences

syn region nicklString start='"' end='"'

let b:current_syntax = "nickl"

hi def link nicklKeyword    Keyword
hi def link nicklNumber     Number
hi def link nicklString     String
hi def link nicklComment    Comment
hi def link nicklFunction   Function
hi def link nicklSymbol     PreProc
hi def link nicklDollar     PreProc
hi def link nicklAt         PreProc
hi def link nicklType       Type
hi def link nicklBoolean    Boolean
hi def link nicklComptime   Function
