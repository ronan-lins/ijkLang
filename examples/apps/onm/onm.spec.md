# Application Specification: onm — Object Notation Management

Develop an educational application named **onm — Object Notation Management**, with an interactive command-line interface for navigating and manipulating JSON objects. The application must support creating, viewing, renaming, updating, and deleting elements, storing its data in an `onm.data` file located in the working directory.

At startup, the application must display its name, version, and instructions for accessing the `help` command. It must then open an interactive session positioned at the root of the JSON object. The prompt must indicate the current location, such as `onm:/>` or `onm:/course/teacher>`. The session must remain open until the user executes `exit`.

## Data Organization

The model must distinguish between two property types: a **node**, whose value is a JSON object, and a **key**, whose value is a string. The root must be an unnamed JSON object and must not be renamed or deleted.

Arrays, numbers, booleans, and `null` values must not be accepted. Values supplied to key creation and update commands must be stored as strings. For example, the value `25` must be persisted as `"25"`.

Nodes and keys must share the same namespace within each object. Therefore, a key and a node cannot have the same name at the same level. Name comparisons must be case-sensitive.

The file must follow this structure:

```json
{
  "course": {
    "name": "JSON fundamentals",
    "teacher": {
      "name": "Ana"
    }
  }
}
```

In this example, `course` and `teacher` are nodes, while the `name` properties are keys.

## Navigation and Commands

Operations must act on the current node or its immediate children. Navigation must proceed one level at a time: `open <name>` enters a child node, and `close` returns to the parent node. When executed at the root, `close` must inform the user that the session is already at the root and leave the current location unchanged.

Implement the following commands:

| Command                        | Behavior                                               |
| ------------------------------ | ------------------------------------------------------ |
| `add node <name>`              | Create an empty node within the current node.          |
| `add key <name> [value]`       | Create a key; when the value is omitted, store `""`.   |
| `list nodes`                   | List the names of immediate child nodes.               |
| `list keys`                    | List the names and values of keys in the current node. |
| `ren node <name> <new_name>`   | Rename a child node while preserving its contents.     |
| `ren key <name> <new_name>`    | Rename a key while preserving its value.               |
| `update <key> <new_value>`     | Change the value of an existing key.                   |
| `del node <name>`              | Physically delete a node and all its contents.         |
| `del key <name>`               | Physically delete a key.                               |
| `open <name>`                  | Enter a child node.                                    |
| `close`                        | Return to the parent node.                             |
| `show node <name> [recursive]` | Display a child node’s contents as JSON.               |
| `show node . [recursive]`      | Display the current node’s contents as JSON.           |
| `help [command]`               | Display general help or help for a specific command.   |
| `exit`                         | End the interactive session.                           |

Items enclosed in `< >` represent required arguments; items enclosed in `[ ]` represent optional arguments. These delimiters are not part of the commands entered by the user.

Arguments containing spaces must be enclosed in double quotation marks:

```text
add node "personal information"
add key name "Ana Silva"
update name "Ana Maria Silva"
```

When a required argument is missing, the application must request it interactively. Omitted optional arguments must use their default behavior without additional prompts.

All commands, messages, and help text must be in English. There must be no multilingual support or `config.json` file.

## Display and Deletion

The `show node` command must print the selected object’s contents without including the property that contains it.

Without the `recursive` argument, it must display the node’s keys and represent its child nodes as empty objects:

```json
{
  "name": "JSON fundamentals",
  "teacher": {}
}
```

With `recursive`, it must display the complete contents of the node and all its descendants:

```json
{
  "name": "JSON fundamentals",
  "teacher": {
    "name": "Ana"
  }
}
```

Deletion must be physical. Executing `del node <name>` must remove the node and all its descendants without an additional confirmation prompt. There must be no logical deletion or restoration mechanism.

## Persistence and Integrity

Every successful creation, rename, update, or deletion must be persisted automatically. There must be no save command, and ending the session must not be necessary to write changes.

At startup, if `onm.data` does not exist, the application must create it with the contents `{}`. The file must use UTF-8 encoding, two-space indentation, and a trailing newline.

For every data operation, the application must open, read, validate, and close the file. For modification operations, it must also persist the result before confirming success. Valid external changes must therefore be recognized on the next operation without restarting the session.

Writes must be atomic, preventing failures from leaving the file partially written. Temporary files used for this purpose must not remain after a completed operation.

If the file contains invalid JSON, duplicate properties, or unsupported types, the operation must be rejected with an explanatory message. The file must not be overwritten. The session must remain available for another attempt after the file has been corrected externally.

If an external change removes the current node or changes it into something other than an object, the application must return to the root and report the location change.

Assume only one running instance, with no concurrent access.

## Validation and Usability

Before modifying data, the application must validate the arguments, the target element’s existence, and its type. Opening a key as a node, updating a node as a key, or creating a property with a name already used in the same object must not be allowed.

Empty names must be rejected. The name `.` must be reserved to represent the current node in the `show node` command.

Syntax, validation, and persistence errors must explain the cause and the action needed to proceed, without ending the session or displaying internal stack traces. A failed operation must not modify the data.

Responses must be brief and consistent. Empty listings must be explicitly identified, and a modification must only be reported as successful after it has been persisted. Help must provide sufficient syntax descriptions, argument explanations, and examples for users to learn how to operate the application directly through the CLI.

## Implementation Guidelines and Grammar Standardization in ijkLang

When implementing the application in **ijkLang (ijkL)**, the following architectural and syntactic standards must be strictly followed:

1. **Universal Standardization `<type> <identifier>`**:
   - All declarations of routine parameters, variables, and constants follow the natural order: `<type> <identifier> [= <initial_value>];`.
   - Routine examples: `public parseCommand(string line): int`, `private printHelp(string command)`.
   - Constant examples: `const string DATA_FILE = "onm.data";`.
   - The delimiter `:` is reserved **strictly and exclusively** for specifying routine return types (`function(type param): return_type`). Procedures (routines without a return value) do not use `:` nor a return type.

2. **Mutability Semantics and Immutable Strings**:
   - The primitive type `string` (`texto`) has **value semantics and is immutable**. No operation mutates strings in-place. Transformation operations like `Text.change(str, idx, c)` or `Text.concat(s1, s2)` allocate and return a new independent string.
   - Structured complex types (such as object or tree node representations) follow **reference semantics** using pointers (`pointer`), allowing safe in-place structural mutations.

