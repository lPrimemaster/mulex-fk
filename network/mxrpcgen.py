#!/usr/bin/python3

# Author : César Godinho
# Date   : 11/10/2024
# Brief  : Detects and parses C++ RPC method declarations
#          (This is NOT a general purpose method parser)

from typing import List, Dict, Tuple
import re
import operator
import os
import argparse
import io
import sys


class RPCMethodType:
    def __init__(self, typename: str, fulltypename: str):
        self.typename = typename
        self.fulltypename = fulltypename
        self._cached_inner_typename = None
        self._is_vector = None

    def is_type(self, typename: str) -> bool:
        return self.fulltypename == typename

    def is_reference(self) -> bool:
        return '&' in self.fulltypename

    def is_generic(self) -> bool:
        return self.is_type('mulex::RPCGenericType')


class RPCMethodArg:
    def __init__(self, name: str, typename: RPCMethodType):
        self.name = name
        self.typename = typename


# BUG: This does not support arguments that use
#      `using namespace x;` on their scope
#      For now just use the full typenames on
#      RPC method calls
class RPCMethodDetails:
    def __init__(self,
                 name: str,
                 fullname: str,
                 rettype: RPCMethodType,
                 args: List[RPCMethodArg]):
        self.name = name
        self.fullname = fullname
        self.rettype = rettype
        self.args = args

    def __str__(self):
        print_lines = [
            f'Name: {self.name}',
            f'Fullname: {self.fullname}',
            'Return Type:',
            f'\tFull typename: {self.rettype.typename}',
            f'\t     Typename: {self.rettype.typename}',
            'Arguments:',
        ]
        for i, arg in enumerate(self.args):
            print_lines.append(f'\t[{i}]')
            print_lines.append(f'\t         Name: {arg.name}')
            print_lines.append(f'\tFull typename: {arg.typename.fulltypename}')
            print_lines.append(f'\t     Typename: {arg.typename.typename}\n')
        if not self.args:
            print_lines.append('\tNone')
        return '\n'.join(print_lines)


class RPCFileParser:
    def __init__(self, filenames: List[str]):
        # Define all the variables to parse the file
        self.rpc_call_keyword = '${RPC_CALL_KEYWORD}'
        self.filenames = filenames
        self.rpc_methods = {}

    def parse(self) -> Dict[str, List[RPCMethodDetails]]:
        total_calls = 0
        for file in self.filenames:
            self.rpc_methods[file] = self._parse_file(file)
            total_calls += len(self.rpc_methods[file])
        print(f'[mxrpcgen] Done. Total parsed RPC calls: {total_calls}')
        return self.rpc_methods

    def _load_file(self, filename: str) -> List[str]:
        with open(filename, 'r', encoding='utf-8') as f:
            return f.readlines()

    def _parse_file(self, filename: str) -> List[RPCMethodDetails]:
        lines = self._load_file(filename)
        def_lines = self._find_def_lines(lines)
        scopes = self._find_lines_scope(lines, def_lines)
        rpc_methods = []
        for line, scope in scopes.items():
            method_details = self._find_rpc_declaration(lines[line], scope)
            rpc_methods.append(method_details)
            print(f'[mxrpcgen] {filename}: '
                  f'registering method: {method_details.fullname}')
            # print(method_details)
        return rpc_methods

    def _find_def_lines(self, lines: List[str]) -> List[int]:
        lines_with_def = []
        for i, line in enumerate(lines):
            if self.rpc_call_keyword in line and not line.strip().startswith('//'):
                lines_with_def.append(i)
        return lines_with_def

    # BUG: This does not work for scenarios like
    #      `namespace x { } CALL_DESC <func_dec>;`
    #      or
    #      `namespace x { class y { CALL_DESC <func_dec>; } }`
    def _find_lines_scope(self, lines: List[str], indices: List[int]) -> Dict[int, str]:
        scope_dict = {}
        scope_name_stack = []
        scope_stack = 0

        for i, line in enumerate(lines):
            for kw in ['namespace', 'class', 'struct']:
                match = re.findall(fr'^[ \t]*{kw} +([a-zA-Z0-9_]*)', line)
                if match:
                    # This is a forward declaration: skip it
                    if re.findall(fr'^[ \t]*{kw} +([a-zA-Z0-9_]*);', line):
                        continue
                    scope_name_stack.append((match[0], scope_stack + 1))

                if '{' in line:
                    scope_stack += 1
                if '}' in line:
                    if len(scope_name_stack) > 0:
                        if scope_name_stack[-1][1] == scope_stack:
                            scope_name_stack.pop()
                    scope_stack -= 1

                if match:
                    break
            if i in indices:
                scope_dict[i] = '::'.join(
                    map(operator.itemgetter(0), scope_name_stack)
                )

        return scope_dict

    def _find_rpc_declaration(self, line: str, scope: str) -> RPCMethodDetails:
        declaration = re.findall(
            fr'^[ \t]*{self.rpc_call_keyword} +(.+) +(.+)\((.*)\)',
            line
        )

        try:
            return_type = self._parse_return_type(declaration[0][0])
        except Exception as e:
            print(e)
            print(f'[mxrpcgen]\tat: {line.strip()}')
            sys.exit(1)

        method_name = declaration[0][1].strip()

        try:
            arguments = self._parse_arguments(declaration[0][2])
        except Exception as e:
            print(e)
            print(f'[mxrpcgen]\tat: {line.strip()}')
            sys.exit(1)

        if scope:
            return RPCMethodDetails(
                method_name,
                f'{scope}::{method_name}',
                return_type,
                arguments
            )

        return RPCMethodDetails(
            method_name,
            method_name,
            return_type,
            arguments
        )

    def _parse_return_type(self, return_type: str) -> RPCMethodType:
        typenames = return_type.split()
        rtype = RPCMethodType(
            typenames[-1].strip(),
            ' '.join(typenames).strip()
        )

        if rtype.is_reference():
            raise Exception('[mxrpcgen] Error, return types '
                            'can not be references.')
        return rtype

    def _parse_arguments(self, arguments: str) -> List[RPCMethodArg]:
        args_parsed = []
        if arguments:
            for arg in arguments.split(','):
                kwords = arg.strip().split()
                typenames = kwords[:-1]
                name = kwords[-1]
                args_parsed.append(RPCMethodArg(
                    name,
                    RPCMethodType(typenames[-1], ' '.join(typenames))
                ))
                if args_parsed[-1].typename.is_reference():
                    raise Exception('[mxrpcgen] Error, parameter types '
                          'can not be references.')
        return args_parsed


class RPCGenerator:
    def __init__(self, methods: Dict[str, List[RPCMethodDetails]]):
        self.buffer = io.StringIO()
        self.methods = methods

    def _write_newline(self) -> None:
        self.buffer.write('\n')

    def _write_indented(self, indent: int, value: str) -> None:
        self.buffer.write('\t'*indent)
        self.buffer.write(value)

    def _generate_method_id_name(self, fullname: str) -> str:
        return 'RPC_CALL_' + fullname.replace('::', '_').upper()

    def _generate_includes(self) -> None:
        anylen = False
        self.buffer.write('#include <cstdint>\n')
        self.buffer.write('#include <vector>\n')
        self.buffer.write('#include <tracy/Tracy.hpp>\n')
        self._write_newline()
        for file, methods in self.methods.items():
            if len(methods):
                anylen = True
                abs_path = os.path.abspath(file)
                self.buffer.write(f'#include "{abs_path}"\n')
        if anylen:
            self._write_newline()

    def _generate_ids(self) -> List[Tuple[RPCMethodDetails, int, int]]:
        id_table = []
        id = 0
        anym = False
        for methods in self.methods.values():
            for method in methods:
                anym = True
                mid = self._generate_method_id_name(method.fullname)
                id_table.append((method, mid, id))
                self.buffer.write(f'#define {mid} {id}\n')
                id += 1
        if anym:
            self._write_newline()
        return id_table

    def _generate_name_lookup(self, idt: List[Tuple[RPCMethodDetails, int, int]]) -> None:
            # Generate a std::map to look for the key (might be slower for less entries)
            self._write_newline()
            self._write_indented(0, 'namespace\n')
            self._write_indented(0, '{\n')
            self._write_indented(1, 'std::uint16_t RPCGetMethodId(const std::string& name)\n')
            self._write_indented(1, '{\n')
            self._write_indented(2, 'static const std::unordered_map<std::string, std::uint16_t> _map = {\n')
            for mname, _, id in idt:
                self._write_indented(3, f'{{\"{mname.fullname}\", {id}}},\n')
            self._write_indented(2, '};\n')
            self._write_indented(2, 'auto it = _map.find(name);\n')
            self._write_indented(2, 'if(it != _map.end())\n')
            self._write_indented(2, '{\n')
            self._write_indented(3, 'return it->second;\n')
            self._write_indented(2, '}\n')
            self._write_indented(2, 'return static_cast<std::uint16_t>(-1);\n')
            self._write_indented(1, '}\n')
            self._write_indented(0, '}\n')
            self._write_newline()

    def _generate_name_list(self, idt: List[Tuple[RPCMethodDetails, int, int]]) -> None:
        # Generate a std::vector with all the keys
        self._write_newline()
        self._write_indented(0, 'namespace\n')
        self._write_indented(0, '{\n')
        self._write_indented(1, 'std::vector<std::string> RPCGetMethods()\n')
        self._write_indented(1, '{\n')
        self._write_indented(2, 'static const std::vector<std::string> _list = {\n')
        for mname, _, _ in idt:
            self._write_indented(3, f'\"{mname.fullname}\",\n')
            self._write_indented(3, f'\"{mname.rettype.fulltypename}\",\n')
        self._write_indented(2, '};\n')
        self._write_indented(2, 'return _list;\n')
        self._write_indented(1, '}\n')
        self._write_indented(0, '}\n')
        self._write_newline()

    def _generate_case(self, idt: Tuple[RPCMethodDetails, int, int]) -> None:
        method, mid, _ = idt
        self._write_indented(3, f'case {mid}:\n')
        self._write_indented(3, '{\n')

        # Static assertion checks for trivially copyable
        # for non void and non generic types
        ret_is_void = method.rettype.is_type('void')
        ret_is_gene = method.rettype.is_generic()
        if not ret_is_void and not ret_is_gene:
            self._write_indented(
                4, 'static_assert(std::is_trivially_copyable_v<'
                   f'{method.rettype.fulltypename}>, "RPC return type must be '
                   'trivially copyable. But is of type: '
                   f'{method.rettype.fulltypename}");\n')

        if len(method.args) > 0:
            for arg in method.args:
                if not arg.typename.is_generic():
                    self._write_indented(
                        4, 'static_assert(std::is_trivially_copyable_v<'
                           f'{arg.typename.fulltypename}>, '
                           '"RPC parameter type must be '
                           'trivially copyable. But is of type: '
                           f'{arg.typename.fulltypename}");\n')

        # Argument offsets
        if len(method.args) > 1:
            for i, arg in enumerate(method.args[:-1]):
                if i == 0:
                    if arg.typename.is_generic():
                        self._write_indented(
                            4,
                            'const std::uint64_t o1 = '
                            f'*reinterpret_cast<const std::uint64_t*>(args) + sizeof(std::uint64_t);\n'
                        )
                    else:
                        self._write_indented(
                            4,
                            'constexpr std::uint64_t o1 = '
                            f'sizeof({arg.typename.fulltypename});\n'
                        )
                else:
                    if arg.typename.is_generic():
                        self._write_indented(
                            4,
                            f'const std::uint64_t o{i + 1} = o{i} + '
                            f'*reinterpret_cast<const std::uint64_t*>(args + o{i}) + sizeof(std::uint64_t);\n'
                        )
                    else:
                        self._write_indented(
                            4,
                            f'constexpr std::uint64_t o{i + 1} = o{i} + '
                            f'sizeof({arg.typename.fulltypename});\n'
                        )

        # Return type (except void and generic)
        if not ret_is_void and not ret_is_gene:
            self._write_indented(
                4, 'retbuf.resize(sizeof('
                   f'{method.rettype.fulltypename}));\n'
            )
            self._write_indented(
                4, f'{method.rettype.fulltypename}* r = '
                   f'reinterpret_cast<{method.rettype.fulltypename}*>'
                   '(retbuf.data());\n'
            )

        # Invokes the copy constructor
        if ret_is_void:
            self._write_indented(4, f'{method.fullname}(')
        elif ret_is_gene:
            self._write_indented(4, f'mulex::RPCGenericType r = {method.fullname}(')
        else:
            self._write_indented(4, f'*r = {method.fullname}(')
        if len(method.args) > 0:
            self._write_indented(0, '\n')
            for i, arg in enumerate(method.args):
                # Appropriately set the const keyword
                constkw = ''
                if 'const' not in arg.typename.fulltypename:
                    constkw = 'const '

                if i == 0:
                    if arg.typename.is_generic():
                        self._write_indented(
                            5,
                            'mulex::RPCGenericType::FromData(args + sizeof(std::uint64_t), '
                            '*reinterpret_cast<const std::uint64_t*>(args))'
                        )
                    else:
                        self._write_indented(
                            5,
                            '*reinterpret_cast<'
                            f'{constkw}{arg.typename.fulltypename}'
                            '*>(args)'
                        )
                else:
                    if arg.typename.is_generic():
                        self._write_indented(
                            5,
                            f'mulex::RPCGenericType::FromData(args + o{i} + sizeof(std::uint64_t), '
                            f'*reinterpret_cast<const std::uint64_t*>(args + o{i}))'
                        )
                    else:
                        self._write_indented(
                            5,
                            '*reinterpret_cast<'
                            f'{constkw}{arg.typename.fulltypename}'
                            f'*>(args + o{i})'
                        )
                if i != len(method.args) - 1:
                    self._write_indented(0, ',\n')
                else:
                    self._write_indented(0, '\n')
            self._write_indented(4, ');\n')
        else:
            self._write_indented(0, ');\n')

        if ret_is_gene:
            self._write_indented(4, 'std::uint64_t data_size = r._data.size();\n')
            self._write_indented(4, 'retbuf.resize(data_size + sizeof(std::uint64_t));\n')
            self._write_indented(4, 'std::memcpy(retbuf.data(), &data_size, sizeof(std::uint64_t));\n')
            self._write_indented(4, 'std::memcpy(retbuf.data() + sizeof(std::uint64_t), r._data.data(), data_size);\n')

        self._write_indented(4, 'break;\n')
        self._write_indented(3, '};\n')

    def _generate_call_lookup(self,
                              ids: List[Tuple[RPCMethodDetails, int, int]]):
        self.buffer.write('namespace\n')
        self.buffer.write('{\n')
        self._write_indented(
            1,
            'std::vector<std::uint8_t> RPCCallLocally(std::uint16_t pid, '
            'const std::uint8_t* args)\n')
        self._write_indented(1, '{\n')
        self._write_indented(2, 'ZoneScoped;\n')
        self._write_indented(2, 'std::vector<std::uint8_t> retbuf;\n')
        self._write_indented(2, 'switch(pid)\n')
        self._write_indented(2, '{\n')

        for id in ids:
            self._generate_case(id)

        self._write_indented(2, '}\n')
        self._write_indented(2, 'return retbuf;\n')
        self._write_indented(1, '}\n')
        self.buffer.write('}\n')

    def _write_file(self, filename: str) -> None:
        with open(filename, 'w') as f:
            f.write(self.buffer.getvalue())

    def generate_rpc_header(self, filename: str):
        self._generate_includes()
        ids = self._generate_ids()
        self._generate_name_lookup(ids)
        self._generate_name_list(ids)
        self._generate_call_lookup(ids)
        self._write_file(filename)


def get_files(args: argparse.Namespace) -> List[str]:
    files_to_parse = []
    for dir in args.dirs:
        for root, _, filenames in os.walk(args.dirs[0]):
            ignoreDir = False
            for ignore in args.ignore:
                if ignore in os.path.basename(root) or ignore in root:
                    ignoreDir = True
                    break
            if ignoreDir:
                continue

            for file in filenames:
                if file.lower().endswith(tuple(args.extensions)):
                    files_to_parse.append(os.path.join(root, file))

            # Only look at root directly
            if not args.recursive:
                break
    return files_to_parse


if __name__ == '__main__':
    arg_parser = argparse.ArgumentParser(
        prog='mxrpcgen',
        description='Generates RPC calls declarators and code logic.'
    )

    arg_parser.add_argument('--extensions', nargs='+', default=['.h'],
                            help='Specifies which file extensions '
                            'to look for.')
    arg_parser.add_argument('--dirs', nargs='+', default=['.'],
                            help='Specifies which directories to '
                            'search for `extensions`.')
    arg_parser.add_argument('--recursive',
                            action='store_true',
                            help='Recursively finds files under '
                            'nested directories of `dirs`.')
    arg_parser.add_argument('--ignore', nargs='+', default=[],
                            help='Specifies folder names to '
                            'ignore. Similar to .gitignore.')

    arg_parser.add_argument('--output-file', required=True,
                            help='Specifiese the output filename.')

    args = arg_parser.parse_args()
    files_to_parse = get_files(args)
    print('[mxrpcgen] Files to parse:')
    for pfile in files_to_parse:
        print(f'[mxrpcgen] \t{pfile}')

    file_parser = RPCFileParser(files_to_parse)
    rpc_methods = file_parser.parse()
    generator = RPCGenerator(rpc_methods)
    generator.generate_rpc_header(args.output_file)
