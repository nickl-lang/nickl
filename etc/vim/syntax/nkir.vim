if exists("b:current_syntax")
  finish
endif

syn keyword nkirKeyword null proc extern cdecl type data const pub local include
syn keyword nkirType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 void ptr
syn keyword nkirBoolean true false

syn match nkirComment    display '//.*$'
syn match nkirFunction   display '\<[a-zA-Z_]\+[(]'me=e-1
syn match nkirSymbol     display '#\w\+'
syn match nkirDollar     display '\$\w\+'
syn match nkirAt         display '@\w\+'
syn match nkirComptime   display '\<\w\+\s*::'me=e-2
syn match nkirNumber     display '\(0x[0-9a-fA-F]\+\|\.[0-9]\+\|\<[0-9]\+\.\=\d*\)\([eE][-+]\=\d\+\>\)\='

syn region nkirComment   start="/\*"    end="\*/"

" TODO Escape sequences

syn region nkirString start='"' end='"'

let b:current_syntax = "nkir"

hi def link nkirKeyword    Keyword
hi def link nkirNumber     Number
hi def link nkirString     String
hi def link nkirComment    Comment
hi def link nkirFunction   Function
hi def link nkirSymbol     PreProc
hi def link nkirDollar     PreProc
hi def link nkirAt         PreProc
hi def link nkirType       Type
hi def link nkirBoolean    Boolean
hi def link nkirComptime   Function
