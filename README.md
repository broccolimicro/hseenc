# HSEenc

**Usage**: hseenc [options] file...
A state encoder and conflict solver for HSE

**General Options**:
 -h,--help      Display this information
    --version   Display version information
    -v,--verbose   Display verbose messages
    -d,--debug     Display internal debugging messages

**Conflict Checking**:
 -c             check for state conflicts that occur regardless of sense
 -cu            check for state conflicts that occur due to up-going transitions
 -cd            check for state conflicts that occur due to down-going transitions
 -s             check for potential state conflicts that occur regardless of sense
 -su            check for potential state conflicts that occur due to up-going transitions
 -sd            check for potential state conflicts that occur due to down-going transitions

## Interactive Environment

<arg> specifies a required argument
(arg=value) specifies an optional argument with a default value

**General**:
 help, h                       print this message
 quit, q                       exit the interactive simulation environment
 load (filename)               load an hse, default is to reload current file
 save (filename)               save an hse, default is to overwrite current file

**Viewing and Manipulating State**:
 elaborate, e                  elaborate the predicates
 conflicts, c                  check for state conflicts that occur regardless of sense
 conflicts up, cu              check for state conflicts that occur due to up-going transitions
 conflicts down, cd            check for state conflicts that occur due to down-going transitions
 suspects, s                   check for potential state conflicts that occur regardless of sense
 suspects up, su               check for potential state conflicts that occur due to up-going transitions
 suspects down, sd             check for potential state conflicts that occur due to down-going transitions

**Viewing and Manipulating Structure**:
 print, p                      print the current hse
 insert <expr>                 insert the transition <expr> into the hse
 pinch <node>                  remove <node> and merge its input and output nodes to maintain token flow
