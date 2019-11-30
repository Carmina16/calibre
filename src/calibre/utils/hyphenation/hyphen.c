/*
 * hyphen.c
 * Copyright (C) 2019 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include <Python.h>
#include <hyphen.h>

#ifdef _MSC_VER
#define fdopen _fdopen
#endif

#define CAPSULE_NAME "hyphen-dict"

void
free_dict(PyObject *capsule) {
	HyphenDict *dict = PyCapsule_GetPointer(capsule, CAPSULE_NAME);
	if (dict) hnj_hyphen_free(dict);
}


static PyObject*
load_dictionary(PyObject *self, PyObject *args) {
	int fd;
	if (!PyArg_ParseTuple(args, "i", &fd)) return NULL;
	FILE *file = fdopen(fd, "rb");
	if (!file) return PyErr_SetFromErrno(PyExc_OSError);
	HyphenDict *dict = hnj_hyphen_load_file(file);
	if (!dict) {
		fclose(file);
		PyErr_SetString(PyExc_ValueError, "Failed to load hyphen dictionary from the specified file");
		return NULL;
	}
	PyObject *ans = PyCapsule_New(dict, CAPSULE_NAME, free_dict);
	if (!ans) fclose(file);
	return ans;
}

HyphenDict*
get_dict_from_args(PyObject *args) {
	if (PyTuple_GET_SIZE(args) < 1) { PyErr_SetString(PyExc_TypeError, "dictionary argument required"); return NULL; }
	return PyCapsule_GetPointer(PyTuple_GET_ITEM(args, 0), CAPSULE_NAME);
}


static PyObject*
simple_hyphenate(PyObject *self, PyObject *args) {
    char hyphenated_word[2*MAX_CHARS], hyphens[MAX_CHARS * 3] = {0}, *word_str;
	PyObject *dict_obj;

	HyphenDict *dict = get_dict_from_args(args);
	if (!dict) return NULL;
    if (!PyArg_ParseTuple(args, "Oes", &dict_obj, &dict->cset, &word_str)) return NULL;
    size_t wd_size = strlen(word_str), hwl = 0;

    if (wd_size >= MAX_CHARS) {
        PyErr_Format(PyExc_ValueError, "Word to be hyphenated (%s) may have at most %u characters, has %zu.", word_str, MAX_CHARS-1, wd_size);
        PyMem_Free(word_str);
        return NULL;
    }

	// we use the simple (old) algorithm since we dont handle replacements
	// anyway
    if (hnj_hyphen_hyphenate(dict, word_str, wd_size, hyphens)) {
        PyErr_Format(PyExc_ValueError, "Cannot hyphenate word: %s", word_str);
    } else {
		for (size_t i = 0; i < wd_size; i++) {
			if (hyphens[i] & 1) {
				hyphenated_word[hwl++] = '=';
			}
			hyphenated_word[hwl++] = word_str[i];
		}
	}
	PyMem_Free(word_str);
	if (PyErr_Occurred()) return NULL;

	return PyUnicode_Decode(hyphenated_word, hwl, dict->cset, "replace");
}


// Boilerplate {{{
static char doc[] = "Wrapper for the hyphen C library";
static PyMethodDef methods[] = {
    {"load_dictionary", (PyCFunction)load_dictionary, METH_VARARGS,
     "load_dictionary(fd) -> Load the specified hyphenation dictionary from the file descriptor which must have been opened for binary reading"
    },
    {"simple_hyphenate", (PyCFunction)simple_hyphenate, METH_VARARGS,
     "simple_hyphenate(dict, unicode_word) -> Return hyphenated word or raise ValueError"
    },

    {NULL}  /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
#define INITERROR return NULL
#define INITMODULE PyModule_Create(&module)
static struct PyModuleDef module = {
    /* m_base     */ PyModuleDef_HEAD_INIT,
    /* m_name     */ "hyphen",
    /* m_doc      */ doc,
    /* m_size     */ -1,
    /* m_methods  */ methods,
    /* m_slots    */ 0,
    /* m_traverse */ 0,
    /* m_clear    */ 0,
    /* m_free     */ 0,
};
CALIBRE_MODINIT_FUNC PyInit_hyphen(void) {
#else
#define INITERROR return
#define INITMODULE Py_InitModule3("hyphen", methods, doc)
CALIBRE_MODINIT_FUNC inithyphen(void) {
#endif

    PyObject* m;

    m = INITMODULE;
    if (m == NULL) {
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
// }}}