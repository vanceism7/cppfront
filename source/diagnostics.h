
//  Copyright (c) Herb Sutter
//  SPDX-License-Identifier: CC-BY-NC-ND-4.0

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


//===========================================================================
//  Aggregate compiler diagnostics
//===========================================================================

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "sema.h"
#include <filesystem>

namespace cpp2 {

    /** A type that holds info about a declaration/symbol in the source */
    struct diagnostic_symbol_t {
        std::string             symbol;
        std::string             kind;
        std::string             scope;
        cpp2::source_position   position;

        auto operator<=>(const diagnostic_symbol_t& other) const = default;
    };

    /** A type denoting a range of text in the source */
    struct diagnostic_scope_range_t {
        cpp2::source_position   start;
        cpp2::source_position   end;
    };
    using diagnostic_scope_map = std::unordered_map<std::string, diagnostic_scope_range_t>;

    /** The main diagnostics type used to communicate compiler results to external programs */
    struct diagnostics_t {
        std::set<diagnostic_symbol_t>   symbols;
        std::vector<cpp2::error_entry>  errors;
        diagnostic_scope_map            scope_map;
    };

    /** Determine the kind of declaration we have */
    auto get_declaration_kind(const cpp2::declaration_node* decl ) -> std::string {
        if (decl->is_function()) return "function";
        if (decl->is_object()) return "var";
        if (decl->is_type()) return "type";
        if (decl->is_namespace()) return "namespace";
        return "unknown";
    }

    /** Get the identifier/name of the declaration */
    auto get_decl_name(const cpp2::declaration_node* decl) -> std::string {
        if (decl == nullptr || decl->identifier == nullptr ) return std::string();

        return decl->identifier->to_string();
    }

    /** Read a declaration_sym into a diagnostic_symbol_t */
    auto read_symbol(const cpp2::declaration_sym* sym) -> diagnostic_symbol_t {
        return diagnostic_symbol_t{
            sym->identifier->to_string(),
            get_declaration_kind(sym->declaration),
            get_decl_name(sym->declaration->get_parent()),
            sym->declaration->position()
        };
    } 
    
    /** Takes a filename + `sema` and aggregates all the diagnostics info */
    auto get_diagnostics(const cpp2::sema& sema) -> diagnostics_t {
        std::set<diagnostic_symbol_t>   symbols     = {};
        diagnostic_scope_map            scope_map   = {};

        // Gather together all of the identifier declarations, along with their position
        for (auto& d : sema.declaration_of) {
            symbols.emplace(read_symbol(d.second.sym));
        } 

        // Gather together the scope ranges for all of our function-like declarations
        // Keep a stack of all of the scopes we've seen
        auto current = std::vector<std::string>{};
        for(auto& s : sema.symbols) 
        {
            switch (s.sym.index()) 
            {
                break; case symbol::active::declaration: 
                {
                    auto const& sym = std::get<symbol::active::declaration>(s.sym);
                    assert (sym.declaration);
                    if( sym.declaration->is_function() || sym.declaration->is_namespace() ) {
                        if( sym.identifier == nullptr ) break;

                        auto name = sym.identifier->to_string();
                        auto pos = sym.position();
                        current.push_back(name);
                        scope_map[name] = diagnostic_scope_range_t{pos, pos};
                    }
                }

                break; case symbol::active::compound: 
                {
                    auto const& sym = std::get<symbol::active::compound>(s.sym);
                    if (!sym.start && sym.kind_ == sym.is_scope && sym.compound != nullptr) 
                    {
                        auto name = current.back();
                        scope_map[name].end = sym.compound->close_brace;
                        current.pop_back();
                    }
                }

                break; default: break;
            }
        }

        return diagnostics_t{symbols, sema.errors, scope_map};
    }

    /** Sanitize a string to make sure its json parsable */
    auto sanitize_for_json(const std::string& s) -> std::string {
        std::string result = s;
    
        // Replace special characters with their escaped versions
        cpp2::replace_all(result, "\\", "\\\\");  // Escape backslash
        cpp2::replace_all(result, "\"", "\\\"");  // Escape double quotes
        cpp2::replace_all(result, "\b", "\\b");   // Escape backspace
        cpp2::replace_all(result, "\f", "\\f");   // Escape formfeed
        cpp2::replace_all(result, "\n", "\\n");   // Escape newlines
        cpp2::replace_all(result, "\r", "\\r");   // Escape carriage return
        cpp2::replace_all(result, "\t", "\\t");   // Escape tab
    
        return result;
    }

    /** Prints the compiler diagnostics to an ostream (either stdout or file) */
    auto print_diagnostics(std::ostream &o, diagnostics_t diagnostics) -> void {

        // Print out symbol info to json
        o << "{\"symbols\": [";
        for(auto& d : diagnostics.symbols) {
            o 
                << "{ \"symbol\": \"" << d.symbol << "\", "
                << "\"scope\": \"" << d.scope << "\", "
                << "\"kind\": \"" << d.kind << "\", "
                << "\"lineno\": " << d.position.lineno << ", "
                << "\"colno\": " << d.position.colno << "},";
        }
        
        // Print out error entries to json
        o << "], \"errors\": [";
        for(auto& e : diagnostics.errors) {
            o
                << "{\"symbol\": \"" << e.symbol << "\", "
                << "\"lineno\": " << e.where.lineno << ", "
                << "\"colno\": " << e.where.colno << ", "
                << "\"msg\": \"" << sanitize_for_json(e.msg) << "\"},";
        }

        // Print out the our scope's source ranges as a map/object where:
        // keys   - are the scope symbol names,
        // values - are their source range
        //
        o << "], \"scopes\": {";
        for(auto& s : diagnostics.scope_map) {
            o
                << "\"" << s.first << "\":"
                << "{\"start\": { \"lineno\": " << s.second.start.lineno << ", "
                << "\"colno\": " << s.second.start.colno << "},"
                << "\"end\": { \"lineno\": " << s.second.end.lineno << ", "
                << "\"colno\": " << s.second.end.colno << "}},";
        }

        o << "}}";
    }
}

#endif