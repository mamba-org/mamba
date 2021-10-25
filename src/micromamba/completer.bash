R"MAMBARAW(
# >>> umamba completion >>>
_umamba_completions()
{
  COMPREPLY=($(micromamba completer "${COMP_WORDS[@]:1}"))
}

complete -o default -F _umamba_completions micromamba
# <<< umamba completion <<<
)MAMBARAW"
