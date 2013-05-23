/*
    Pythonika 1.0 - Python Interpreter Interface for Mathematica.
    
    Copyright (c) 2005-2010 Ero Carrera <ero@dkbza.org>

    All rights reserved.

*/

#include "Pythonika.h"
#include "mathlink.h"


// Python code run at initialization time to
// set instances of a class to capture the
// stdin and stdout generated by the Python
// code.

const char *interpreter_initialization_code = \
"import sys\n"
"class Capture:\n"
"	def __init__(self):\n"
"		self.data = ''\n"
"	def write(self, str):\n"
"		self.data += str\n"
"stdout = sys.stdout\n"
"stderr = sys.stderr\n"
"capture_stdout = Capture()\n"
"capture_stderr = Capture()\n"
"sys.stdout = capture_stdout\n"
"sys.stderr = capture_stderr\n";


InterpreterState *interpreter_state;

#define MAX_LIST_LEVELS_MSG "Maximum level of 2048 nested lists reached."
#define MAX_LIST_LEVELS     2048
static PyObject *List[MAX_LIST_LEVELS];
static int list_level = -1;


int main(int argc, char **argv)
{

    init_interpreter();
    
    return MLMain(argc, argv);

    shutdown_interpreter();
}


void PyInteger(char const*name, long value)
{
    PyObject *obj;

    obj = PyLong_FromLong(value);
    
    if(list_level>=0 && List[list_level]) {
        PyList_Append(List[list_level], obj);
        Py_DECREF(obj);        
        return;
    }

    PyObject_SetAttrString(interpreter_state->main_module, name, obj);
    Py_DECREF(obj);
}


void PyReal(char const*name, double value)
{
    PyObject *obj;

    obj = PyFloat_FromDouble(value);
    
    if(list_level>=0 && List[list_level]) {
        PyList_Append(List[list_level], obj);
        Py_DECREF(obj);    
        return;
    }

    PyObject_SetAttrString(interpreter_state->main_module, name, obj);
    Py_DECREF(obj);
}


void PyComplex(char const*name, double value, double value2)
{
    PyObject *obj;

    obj = PyComplex_FromDoubles(value, value2);
    
    if(list_level>=0 && List[list_level]) {
        PyList_Append(List[list_level], obj);
        Py_DECREF(obj);    
        return;
    }

    PyObject_SetAttrString(interpreter_state->main_module, name, obj);
    Py_DECREF(obj);
}


void PyUnicodeString(char const*name, unsigned short const*value, long len)
{
    PyObject *obj;
    

    obj = PyUnicode_FromUnicode((unsigned short *)value, len);

    if(list_level>=0 && List[list_level]) {        
        PyList_Append(List[list_level], obj);
        Py_DECREF(obj);
        return;
    }

    PyObject_SetAttrString( interpreter_state->main_module, name, obj);
    Py_DECREF(obj);
}


void PyString(char const*name, int *value, long len)
{
    long idx;
    char *buffer;
    PyObject *obj;
    
    
    buffer = (char *)malloc(len);
    if(!buffer) {
        MLPutString(stdlink, "Pythonika: Can't allocate memory for string.");
        return;
    }

    for(idx=0; idx<len; idx++)
        buffer[idx] = (char) value[idx];


    obj = PyString_FromStringAndSize(buffer, len);
    free(buffer);

    if(list_level>=0 && List[list_level]) {        
        PyList_Append(List[list_level], obj);
        Py_DECREF(obj);
        MLPutSymbol(stdlink, "Null");    
        return;
    }

    PyObject_SetAttrString( interpreter_state->main_module, name, obj);
    Py_DECREF(obj);
        
    MLPutSymbol(stdlink, "Null");
}


void PySymbol(char const*name, char const*value)
{
    PyObject *obj;
    
    if(!strncmp(value, "Null", 4))
        obj = Py_None;
    else if(!strncmp(value, "True", 4))
        obj = Py_True;
    else if(!strncmp(value, "False", 5))
        obj = Py_False;

    if(list_level>=0 && List[list_level]) {        
        PyList_Append(List[list_level], obj);
        return;
    }

    PyObject_SetAttrString( interpreter_state->main_module, name, obj);
}


void PyOpenList(char const*name)
{
    PyObject *list;
    
    if(list_level+1==MAX_LIST_LEVELS) {
        MLPutString(stdlink, MAX_LIST_LEVELS_MSG);
        return;
    }

    list = PyList_New(0);

    // If a list exists, embed this list inside
    if(list_level>=0 && List[list_level])
        PyList_Append(List[list_level], list);
        
    // Otherwise just add a new one using the given name
    else
        PyObject_SetAttrString(interpreter_state->main_module, name, list);

    List[++list_level] = list;

    Py_DECREF(list);
    MLPutSymbol(stdlink, "Null");    
}


void PyCloseList(void)
{
    List[list_level--] = NULL;
}


// Attempt to execute the input typed by the user.

#define MATHEMATICA_NEWLINE_TOKEN   "\\012"
#define NEWLINE_TOKEN   "   \n"

void clean_input(char const*input)
{
    int i, length;

    length = strlen(input);
    
    for(i=0; i+4<=length; i++)
        if(!memcmp( (void *)(input+i), (void *)MATHEMATICA_NEWLINE_TOKEN, 4 ))
            memcpy((void *)(input+i), NEWLINE_TOKEN, 4);           
} 
    

void Py(char const*input) {
    PyObject *code_obj;

    clean_input(input);
    
    // Compile the user input as an expression.  
    code_obj = Py_CompileString(input, "User Input", Py_eval_input);
    
    if(code_obj) {
    
        // If the compilation was successful, the resulting code
        // object is evaluated.
        DEBUG("execute_input: evaluating eval_input")
        
        mat_eval_compiled_code(code_obj, Py_eval_input);
        Py_DECREF(code_obj);
        
        return;
        
    } else {
    
        DEBUG("execute_input: evaluating file_input")
        
        // If the compilation did not succeed probably the
        // code was not an expression. Subsequently it will now
        // be compiled as a statement or group of statements.
        // The error is therefore cleared. If it triggers again
        // after this compilation then there will be a syntax
        // or other kind or error in the user's input.
        PyErr_Clear();
        
        code_obj = Py_CompileString(input, "User Input", Py_file_input);
        if(code_obj) {
            mat_eval_compiled_code(code_obj, Py_file_input);
            Py_DECREF(code_obj);
        
            return;
        }
	}
			
    handle_error();
	return;
}


// Read and output any data which Python code
// might have written to stdout or stderr.

void process_std(PyObject *source)
{
    PyObject *std_data;
    char *std_str;
    Py_ssize_t length;

    DEBUG("process_std: getting data")
    std_data = PyObject_GetAttrString(source, "data");
    DEBUG("process_std: getting string from data")
    PyString_AsStringAndSize(std_data, &std_str, &length);
    
    if(length>0) {
        DEBUG("process_std: showing string")
        MLPutString(stdlink, std_str);
        Py_DECREF(std_data);
        
        PyObject_SetAttrString(source, "data", PyString_FromString(""));
        
        return;
    }
    MLPutSymbol(stdlink, "Null");
}


void flush_std(PyObject *source)
{
    PyObject *std_data;
    char *std_str;

	std_data = PyObject_GetAttrString(source, "data");
	std_str = PyString_AsString(std_data);
	
	if(strlen(std_str)>0) {
        Py_DECREF(std_data);        
        PyObject_SetAttrString(source, "data", PyString_FromString(""));
        return;
    }
}


// Handle errors in the input. If the error is a syntax
// error it might be due to incomplete input. That's
// checked for and if so, more input is requested.

void handle_error(void)
{
    // The error information is written to stdout.
    PyErr_Print();

    process_std(interpreter_state->capture_stderr);
    flush_std(interpreter_state->capture_stdout);
    
    return;
}


void mat_process_iterable_object(PyObject *obj, char *error_msg)
{
        Py_ssize_t length;
        PyObject *iterator, *item;
        
        length = PyObject_Length(obj);

        MLPutFunction(stdlink, "List", 1);
        MLPutFunction(stdlink, "Sequence", length);
        
        iterator = PyObject_GetIter(obj);
        
        if(iterator==NULL) {
            MLPutString(stdlink, error_msg);
            return;
        }
        
        while((item = PyIter_Next(iterator))) {
            python_to_mathematica_object(item);
            Py_DECREF(item);
        }
        
        Py_DECREF(iterator);

        return;
}


void python_to_mathematica_object(PyObject *obj)
{
    if(PyBool_Check(obj)) {
        if(obj==Py_True)
            MLPutSymbol(stdlink, "True");
        else
            MLPutSymbol(stdlink, "False");
        return;
    }
    if(PyInt_Check(obj)) {
        MLPutLongInteger(stdlink, PyInt_AsLong(obj));
        return;
    }
    if(PyLong_Check(obj)) {
        Py_ssize_t length;
        char *str, *mat_expr;
        PyObject *long_as_str;
        
        long_as_str = PyObject_CallMethod(obj, (char *)"__str__", NULL);
        PyString_AsStringAndSize(long_as_str, &str, &length);
        
        MLPutFunction(stdlink, "ToExpression", 1);
        MLPutString(stdlink, str);

        Py_DECREF(long_as_str);
        return;
    }
    if(obj==Py_None) {
        MLPutSymbol(stdlink, "Null");
        return;
    }
    if(PyFloat_Check(obj)) {
        MLPutDouble(stdlink, (double)PyFloat_AsDouble(obj));
        return;
    }
    if(PyComplex_Check(obj)) {
        MLPutFunction(stdlink, "Complex", 2);
        MLPutDouble(stdlink, (double)PyComplex_RealAsDouble(obj));
        MLPutDouble(stdlink, (double)PyComplex_ImagAsDouble(obj));
        return;
    }
    if(PyString_Check(obj)) {
        char *str;
        Py_ssize_t length;
        
        PyString_AsStringAndSize(obj, &str, &length);
        
        MLPutByteString(stdlink, (unsigned char *)str, length);
        return;
    }

    if(PyUnicode_Check(obj)) {
        MLPutUnicodeString(stdlink,
            PyUnicode_AsUnicode(obj),
            PyUnicode_GetSize(obj) );
        return;
    }
    if(PyTuple_Check(obj)) {
        
        mat_process_iterable_object(obj, (char *)"Can't get iterator for 'tuple'");

        return;
    }
    if(PyList_Check(obj)) {
    
        mat_process_iterable_object(obj, (char *)"Can't get iterator for 'list'");

        return;
    }
    if(PyObject_TypeCheck(obj, &PySet_Type)) {
    
        mat_process_iterable_object(obj, (char *)"Can't get iterator for 'set'");
    
        return;
    }

    if(PyDict_Check(obj)) {
        PyObject *items;
        
        items = PyDict_Items(obj);
        python_to_mathematica_object(items);
        Py_DECREF(items);

        return;
    }
    
    // This should ideally print info, like type of the object, that
    // can't be converted.
    MLPutString(stdlink, "Object type can't be converted!");
}



// Evaluate a compiled "code object"

void mat_eval_compiled_code(PyObject *code_obj, int parse_mode)
{
	PyObject *code_executed;

    DEBUG("eval_code: running PyEval_EvalCode")
    
    // Evaluate the code.
    code_executed = (PyObject *)PyEval_EvalCode(
        (PyCodeObject *)code_obj,
        interpreter_state->main_dict,
        interpreter_state->main_dict);

    if(!code_executed) {
        DEBUG("eval_code: PyEval_EvalCode failed")
        handle_error();
        return;
    }
    DEBUG("eval_code: code executed")
    
    DEBUG("eval_code: showing output")
    
    if(parse_mode==Py_eval_input) {
    
        // If the mode is Py_eval_input and the code evaluated
        // successfully, an expression has been executed and the
        // resulting value is the returned Python object.
        // The interpreter will output a string representation of
        // the object.
        DEBUG("eval_code: showing repr")
        
        python_to_mathematica_object(code_executed);
        
        // Mathematica can only receive in one stream (no separate way of
        // feeding stderr/stdour messages). If an object is returned, then
        // any text printed, which would be otherwise returned as a string,
        // is discarded.
        flush_std(interpreter_state->capture_stdout);
        
        // Free unneeded object.
        Py_DECREF(code_executed);
        return;
        
    } else {
    
        // Otherwise, the code evaluated was one or more statements
        // which do not return a value, if any output has been
        // produced, it will be available in the Capture instance
        // The output is shown by the interpreter.

        DEBUG("eval_code: showing stdout")
        process_std(interpreter_state->capture_stdout);
        return;
    }
    
    
    MLPutSymbol(stdlink, "Null");
    
    return;
}



void init_interpreter(void)
{
	PyObject *code_executed;

#ifdef READLINE
    rl_initialize();
    (char*(*)(const char*, int))rl_completion_entry_function = (char*(*)(const char*, int))rl_python_completion_function;
#endif

    Py_Initialize();
    
    interpreter_state = (InterpreterState *)malloc(sizeof(InterpreterState));
    
    // Get the main dictionary
    interpreter_state->main_module  = PyImport_AddModule("__main__");
    interpreter_state->main_dict    = PyModule_GetDict(
                                        interpreter_state->main_module);
    
    code_executed = PyRun_String(
        interpreter_initialization_code, Py_file_input,
        interpreter_state->main_dict, NULL);
    interpreter_state->capture_stdout = PyMapping_GetItemString(
        interpreter_state->main_dict, (char *)"capture_stdout");
    interpreter_state->capture_stderr = PyMapping_GetItemString(
        interpreter_state->main_dict, (char *)"capture_stderr");
                
    Py_DECREF(code_executed);
    
    return;
}


void shutdown_interpreter(void)
{
    Py_DECREF(interpreter_state->capture_stdout);
    Py_DECREF(interpreter_state->capture_stderr);
    
#ifdef READLINE
	Py_DECREF(interpreter_state->__builtins__);
    
    free_array_of_strings(builtins);
    free_array_of_strings(keywords);
#endif

    Py_Finalize();
}
