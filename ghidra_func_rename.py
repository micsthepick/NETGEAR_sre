from ghidra.app.decompiler import DecompInterface, DecompileOptions
from ghidra.program.model.symbol import SourceType, RefType
from ghidra.util.task import ConsoleTaskMonitor
import re

TARGET_FUNC_NAME = "dni_sprintf_s"

def resolve_string_from_varnode(varnode):
    if varnode is None or not varnode.isAddress():
        # Try to follow it if it's not directly an address
        if varnode is None or not varnode.isRegister():
            return None
        # Try to find where this varnode came from
        defs = varnode.getDef()
        if defs:
            for i in range(defs.getNumInputs()):
                result = resolve_string_from_varnode(defs.getInput(i))
                if result:
                    return result
        return None

    addr = varnode.getAddress()
    if addr is None:
        return Nones
    str_val = getDataAt(addr)
    if str_val and str_val.hasStringValue():
        return str(str_val.getValue())
    return None

def get_string_arg_smart(pcode_op, index):
    if index >= pcode_op.getNumInputs():
        return None
    arg = pcode_op.getInput(index)
    return resolve_string_from_varnode(arg)

def find_func_by_name(name):
    fm = currentProgram.getFunctionManager()
    for func in fm.getFunctions(True):
        if func.getName() == name:
            return func
    return None

def get_calling_functions(target_func):
    callers = set()
    refs = getReferencesTo(target_func.getEntryPoint())
    for ref in refs:
        if ref.getReferenceType().isCall():
            from_addr = ref.getFromAddress()
            caller = getFunctionContaining(from_addr)
            if caller:
                callers.add(caller)
    return callers

def get_string_arg(pcode_op, index):
    if index >= pcode_op.getNumInputs():
        return None
    arg = pcode_op.getInput(index)
    addr = arg.getAddress()
    if addr is None:
        print("  Arg[{}] has no address".format(index))
        return None
    str_val = getDataAt(addr)
    if str_val and str_val.hasStringValue():
        print("  Arg[{}] string: {}".format(index, str_val.getValue()))
        return str(str_val.getValue())
    else:
        print("  Arg[{}] not a string or no value: {}".format(index, str_val))
    return None

def should_rename(name):
    return name.startswith("FUN_") or name.startswith("sub_")

def main():
    monitor = ConsoleTaskMonitor()
    decompiler = DecompInterface()
    options = DecompileOptions()
    decompiler.setOptions(options)
    decompiler.openProgram(currentProgram)

    target_func = find_func_by_name(TARGET_FUNC_NAME)
    if not target_func:
        print("Function '{}' not found.".format(TARGET_FUNC_NAME))
        return

    callers = get_calling_functions(target_func)
    print("Found {} functions that call '{}'".format(len(callers), TARGET_FUNC_NAME))

    findre = re.compile(TARGET_FUNC_NAME+r'\("([^"]*)"')

    for func in callers:
        print("\n---\nProcessing {}".format(func.getName()))
        # Ghidra Python script to search for a string in the decompiled functions

        # Decompile the function
        decompiled_function = decompiler.decompileFunction(func, 60, ConsoleTaskMonitor())

        # Check if the decompiled text contains the search term
        if decompiled_function != None:
            c_code = decompiled_function.getDecompiledFunction()
            cont = False
            if c_code is None:
                cont = True
            else:
                c_string = c_code.getC()
                if c_string is None:
                    cont = True
            if cont:
                print('no c code for {}'.format(func.getName()))
                continue
            for match in findre.finditer(c_string):
                print(match)
                new_name = match.group(1).strip().replace(" ", "_")
                try:
                    func.setName(new_name, SourceType.USER_DEFINED)
                    print("  Renamed to '{}'".format(new_name))
                except Exception as e:
                    print("  Rename failed: {}".format(str(e)))
                break  # Only rename based on the first matching call
            else:
                print("  Could not get a valid string to rename")

main()
