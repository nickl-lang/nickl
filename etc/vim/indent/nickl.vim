if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal nosmartindent
setlocal nolisp
setlocal autoindent

setlocal indentexpr=GetNicklIndent(v:lnum)

if exists("*GetNicklIndent")
  finish
endif

function! GetNicklIndent(lnum)
  let prev = prevnonblank(a:lnum-1)

  if prev == 0
    return 0
  endif

  let prevline = getline(prev)
  let line = getline(a:lnum)

  let ind = indent(prev)

  if prevline =~ '[({]\s*$'
    let ind += &sw
  endif

  if line =~ '^\s*[)}]'
    let ind -= &sw
  endif

  return ind
endfunction
