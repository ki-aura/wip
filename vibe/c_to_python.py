#############################
# Python for C Programmers  #
#############################

# Blocks / Scope
# Python uses indentation instead of {}.
# Any line ending with ":" starts a block.

# C:
# if (x) {
#     ...
# }

# Python:
# if x:
#     ...


# Variables
# - No type declarations
# - Dynamically typed
# - Variables are references (like managed pointers)

a = 5


# Conditionals
# C:
# if (x == 5) { ... } else if (x > 0) { ... } else { ... }

# Python:
if x == 5:
    ...
elif x > 0:
    ...
else:
    ...


# Loops

# While loop:
while x < 5:
    ...

# For loop:
# Python "for" is foreach-like; range(n) gives 0..n-1
for i in range(10):
    ...


# Functions
def add(a, b):
    return a + b


# Error Handling
# Python uses exceptions
raise ValueError("Invalid value")


# Pointers
# Python has no raw pointers.
# Variables reference objects and assignments copy the reference.


# Memory Management
# Automatic garbage collection. No malloc/free.


# Byte Arrays
data = bytearray(256)
data[3] = 0x41   # similar to unsigned char buffer


# Strings
# Immutable in Python.
# To change: create a new string
s = s[:3] + "A" + s[4:]


# No switch statement
# Use if/elif/else or a dict lookup.


# Logical Operators
# C: && || !
# Python: and or not


# Truthiness
# Falsy: 0, "", [], {}, None, False
# Everything else is truthy.


# Equality vs Identity
# == compares values
# is compares identity (like pointer equality)


# Structs / Classes
from dataclasses import dataclass

@dataclass
class Point:
    x: int
    y: int


# Modules
# Equivalent to "include + link" but dynamic
import my_module


# Mutability
# Mutable: list, dict, bytearray
# Immutable: int, str, tuple


# Chained comparisons
# Python allows:
if 0 < x < 10:
    ...

# Equivalent in C:
# if (0 < x && x < 10)


# Summary Table:
# Concept    | C                          | Python
# -----------|-----------------------------|----------------------------
# Blocks     | {}                          | indentation
# Loop       | for(init;cond;inc)          | for x in range(n)
# Types      | explicit                    | dynamic
# Pointers   | explicit                    | implicit references
# Memory     | manual                      | automatic GC
# Struct     | struct                      | @dataclass
# Errors     | errno/return                | exceptions
# Strings    | mutable arrays              | immutable objects
# Conditions | if(...)                     | if ...:
# Bool ops   | && || !                     | and or not

#############################
