R"MAMBARAW(
# >>> umamba completion >>>
autoload -U +X compinit && compinit
autoload -U +X bashcompinit && bashcompinit

_umamba_completions()
{
  COMPREPLY=($(micromamba completer "${(@s: :)${(@s: :)COMP_LINE}:1}"))
}

complete -o default -F _umamba_completions micromamba
# <<< umamba completion <<<
)MAMBARAW"
