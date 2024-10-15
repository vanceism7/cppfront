
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
#include <unordered_set>

namespace cpp2 {

    /** A type that holds info about a declaration/symbol in the source */
    struct diagnostic_symbol_t {
        std::string     symbol;
        cpp2::lineno_t  lineno;
        cpp2::colno_t   colno;

        auto operator<=>(const diagnostic_symbol_t& other) const = default;
    };

    /** A type that holds info about an error in the source */
    struct diagnostic_error_t {
        std::string     file;
        std::string     symbol;
        std::string     msg;
        cpp2::lineno_t  lineno;
        cpp2::colno_t   colno;
    };

    /** The main diagnostics type used to communicate compiler results to external programs */
    struct diagnostics_t {
        std::set<diagnostic_symbol_t>   symbols;
        std::vector<diagnostic_error_t> errors;
    };
    
    /** Takes a filename + `sema` and aggregates all the diagnostics info */
    auto get_diagnostics(std::string sourcefile, const cpp2::sema& sema) -> diagnostics_t {
        std::set<diagnostic_symbol_t>   symbols = {};
        std::vector<diagnostic_error_t> errors = {};

        // Gather together all of the identifier declarations, along with their position
        for (auto& d : sema.declaration_of) {
            symbols.emplace(
                d.second.sym->identifier->to_string(),
                d.second.sym->declaration->position().lineno,
                d.second.sym->declaration->position().colno
            );
        } 

        // Gather together all of our errors into a simple error type
        for(auto& e : sema.errors) {
            errors.emplace_back(
                sourcefile,
                e.symbol, 
                e.msg,
                e.where.lineno, 
                e.where.colno
            );
        }

        return diagnostics_t{symbols, errors};
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
        o << "{\"symbols\": [";
        for(auto& d : diagnostics.symbols) {
            o 
                << "{ \"symbol\": \"" << d.symbol << "\", "
                << "\"lineno\": " << d.lineno << ", "
                << "\"colno\": " << d.colno << "},";
        }
        
        o << "], \"errors\": [";
        for(auto& e : diagnostics.errors) {
            o
                << "{\"file\": \"" << e.file << "\", "
                << "\"symbol\": \"" << e.symbol << "\", "
                << "\"lineno\": " << e.lineno << ", "
                << "\"colno\": " << e.colno << ", "
                << "\"msg\": \"" << sanitize_for_json(e.msg) << "\"},";
        }

        o << "]}";
    }
}

#endif