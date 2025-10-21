## Usage
```bash
./flow <flowfile> <pipe_name>
```

## Features
- **Nodes**: Execute single processes
- **Pipes**: Connect stdout of one component to stdin of another  
- **Concatenate**: Sequentially run multiple components and append outputs
- **Stderr**: Capture stderr stream from nodes for processing

## Implementation
Uses fork/exec to create child processes and dup2() for file descriptor redirection. Parsing builds buffered structs that are finalized when complete. A dispatcher pattern routes execution through run_*_into_fd() functions. All components can be chained together through the unified interface.

Uses the `sh -c` approach to directly run commands in shell rather than parsing argv.

## Example
```bash
./flow filecount.flow doit
```

## Files
- `flow.c` - Main interpreter implementation
- `filecount.flow` - Basic ls|wc example
- `complicated.flow` - Concatenate example with nested pipes
- `error_handling.flow` - Stderr capture example
- various other test files
