if exists("b:current_syntax")
  finish
endif

syn match nkstKeyword    display '(\w*'ms=s+1

syn keyword nkstType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 void ptr slice array usize string type_t any_t bool
syn keyword nkstBoolean true false

syn match nkstComment    display '//.*$'
syn match nkstFunction   display '\<\w\+[(]'me=e-1
syn match nkstSymbol     display '#\w\+'
syn match nkstDollar     display '\$\w\+'
syn match nkstAt         display '@\w\+'
syn match nkstComptime   display '\<\w\+\s*::'me=e-2
syn match nkstNumber     display '\(0x[0-9a-fA-F_]\+\|\.[0-9_]\+\|\<[0-9_]\+\.\=\d*\)\([eE][-+]\=\d\+\>\)\='

syn region nkstComment   start="/\*"    end="\*/"

" TODO Escape sequences

syn region nkstString start='"' end='"'

let b:current_syntax = "nkst"

hi def link nkstKeyword    Keyword
hi def link nkstNumber     Number
hi def link nkstString     String
hi def link nkstComment    Comment
hi def link nkstFunction   Function
hi def link nkstSymbol     PreProc
hi def link nkstDollar     PreProc
hi def link nkstAt         PreProc
hi def link nkstType       Type
hi def link nkstBoolean    Boolean
hi def link nkstComptime   Function
