R"MAMBARAW(

if not set -q MAMBA_SHLVL
  set -gx MAMBA_SHLVL "0"
  set -gx PATH $MAMBA_ROOT_PREFIX/condabin $PATH
end

if not set -q MAMBA_NO_PROMPT
  function __mamba_add_prompt
    if set -q MAMBA_PROMPT_MODIFIER
      set_color -o green
      echo -n $MAMBA_PROMPT_MODIFIER
      set_color normal
    end
  end

  if functions -q fish_prompt
      if not functions -q __fish_prompt_orig
          functions -c fish_prompt __fish_prompt_orig
      end
      functions -e fish_prompt
  else
      function __fish_prompt_orig
      end
  end

  function return_last_status
    return $argv
  end

  function fish_prompt
    set -l last_status $status
    if set -q MAMBA_LEFT_PROMPT
        __mamba_add_prompt
    end
    return_last_status $last_status
    __fish_prompt_orig
  end

  if functions -q fish_right_prompt
      if not functions -q __fish_right_prompt_orig
          functions -c fish_right_prompt __fish_right_prompt_orig
      end
      functions -e fish_right_prompt
  else
      function __fish_right_prompt_orig
      end
  end

  function fish_right_prompt
    if not set -q MAMBA_LEFT_PROMPT
        __mamba_add_prompt
    end
    __fish_right_prompt_orig
  end
end


function __mamba_add_sys_prefix_to_path
  test -z "$MAMBA_ROOT_PREFIX" && return

  set -gx MAMBA_INTERNAL_OLDPATH $PATH
  if test -n "$WINDIR"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/bin"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/Scripts"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/Library/bin"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/Library/usr/bin"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/Library/mingw-w64/bin"
    set -gx -p PATH "$MAMBA_ROOT_PREFIX"
  else
    set -gx -p PATH "$MAMBA_ROOT_PREFIX/bin"
  end
end

function micromamba --inherit-variable MAMBA_EXE
  if [ (count $argv) -lt 1 ]
    eval $MAMBA_EXE
  else
    set -l cmd $argv[1]
    set -e argv[1]
    __mamba_add_sys_prefix_to_path
    switch $cmd
      case activate deactivate
        $MAMBA_EXE shell -s fish $cmd $argv | source || return $status
        return
      case install update upgrade remove uninstall
        eval $MAMBA_EXE $cmd $argv || return $status
        set -l script (eval $MAMBA_EXE shell -s fish reactivate) || return $status
        eval $script || return $status
      case '*'
        eval $MAMBA_EXE $cmd $argv || return $status
    end
    set ret $status
    set -gx PATH $MAMBA_INTERNAL_OLDPATH
    return $ret
  end
end



# Autocompletions below

function __fish_mamba_env_commands
  string replace -r '.*_([a-z]+)\.py$' '$1' $MAMBA_ROOT_PREFIX/lib/python*/site-packages/conda_env/cli/main_*.py
end

function __fish_mamba_envs
  micromamba env list | tail -n +11 | cut -d' ' -f3
end

function __fish_mamba_packages
  micromamba list | awk 'NR > 3 {print $1}'
end

function __fish_mamba_universial_optspecs
    string join \n 'h/help' 'a-version' 'b-rc-file' \
                   'c-no-rc' 'd-no-env' 'v/verbose' \
                   'q/quiet' 'y/yes' 'e-json' 'f-offline' \
                   'g-dry-run' 'i-experimental' 'r/root-prefix' \
                   'p/prefix' 'n/name'
end

function __fish_mamba_needs_command
    # Figure out if the current invocation already has a command.
    set -l argv (commandline -opc)
    set -e argv[1]
    argparse -s (__fish_mamba_universial_optspecs) -- $argv 2>/dev/null
    or return 0
    if set -q argv[1]
      echo $argv[1]
      return 1
    end
    return 0
end

function __fish_mamba_using_command
    set -l cmd (__fish_mamba_needs_command)
    test -z "$cmd"
    and return 1
    contains -- $cmd $argv
    and return 0
end


# Conda commands
complete -x -c micromamba -n '__fish_mamba_needs_command' -a shell       -d 'Generate shell init scripts'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a create      -d 'Create new environment'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a install     -d 'Install packages in active environment'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a update      -d 'Update packages in active environment'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a remove      -d 'Remove packages from active environment'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a list        -d 'List packages in active environment'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a clean       -d 'Clean package cache'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a config      -d 'Configuration of micromamba'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a info        -d 'Information about micromamba'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a constructor -d 'Commands to support using micromamba in constructor'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a env         -d 'List environments'
# Commands after hook
complete -x -c micromamba -n '__fish_mamba_needs_command' -a activate    -d 'Commands to support using micromamba in constructor'
complete -x -c micromamba -n '__fish_mamba_needs_command' -a deactivate  -d 'List environments'

# subcommands
complete -x -c micromamba -n '__fish_mamba_using_command env' -a 'list'  -d 'List known environments'

# Commands that need environment as parameter
complete -x -c micromamba -n '__fish_mamba_using_command activate' -a '(__fish_mamba_envs)'

# Commands that need package as parameter
complete -x -c micromamba -n '__fish_mamba_using_command remove' -a '(__fish_mamba_packages)'
complete -x -c micromamba -n '__fish_mamba_using_command uninstall' -a '(__fish_mamba_packages)'
complete -x -c micromamba -n '__fish_mamba_using_command upgrade' -a '(__fish_mamba_packages)'
complete -x -c micromamba -n '__fish_mamba_using_command update' -a '(__fish_mamba_packages)'

)MAMBARAW"
