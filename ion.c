/*
    +--------------------------------------------------------------------+
    | PECL :: ion                                                        |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2021, Michael Wallner <mike@php.net>                 |
    +--------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"

#include "Zend/zend_enum.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_closures.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"

#include "ext/date/php_date.h"
#include "ext/spl/spl_exceptions.h"
#include "ext/spl/spl_iterators.h"

#define DECNUMDIGITS 34 /* DECQUAD_Pmax */
#include "ionc/ion.h"

static decContext g_dec_ctx;
static ION_DECIMAL g_ion_dec_zend_max, g_ion_dec_zend_min;

#include "php_ion.h"
#define ZEND_ARG_VARIADIC_OBJ_TYPE_MASK(pass_by_ref, name, classname, type_mask, default_value) \
	{ #name, ZEND_TYPE_INIT_CLASS_CONST_MASK(#classname, type_mask | _ZEND_ARG_INFO_FLAGS(pass_by_ref, 1, 0)), default_value },
#include "ion_arginfo.h"
#include "ion_private.h"

ZEND_METHOD(ion_Symbol_ImportLocation, __construct)
{
	php_ion_symbol_iloc *obj = php_ion_obj(symbol_iloc, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_long location;
	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(obj->name)
		Z_PARAM_LONG(location)
	ZEND_PARSE_PARAMETERS_END();

	obj->loc.location = location;
	php_ion_symbol_iloc_ctor(obj);
}
ZEND_METHOD(ion_Symbol, __construct)
{
	php_ion_symbol *obj = php_ion_obj(symbol, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_long sid = -1;
	ZEND_PARSE_PARAMETERS_START(0, 3)
		Z_PARAM_OPTIONAL
		Z_PARAM_STR_OR_NULL(obj->value)
		Z_PARAM_LONG(sid)
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->iloc, ce_Symbol_ImportLocation)
	ZEND_PARSE_PARAMETERS_END();

	obj->sym.sid = sid;
	php_ion_symbol_ctor(obj);
}
ZEND_METHOD(ion_Symbol, equals)
{
	php_ion_symbol *sym = php_ion_obj(symbol, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(sym);

	zend_object *other_obj;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(other_obj, ce_Symbol)
	ZEND_PARSE_PARAMETERS_END();

	BOOL eq = FALSE;
	iERR err = ion_symbol_is_equal(
		&sym->sym,
		&php_ion_obj(symbol, other_obj)->sym, &eq);
	ION_CHECK(err);
	RETVAL_BOOL(eq);
}
ZEND_METHOD(ion_Symbol, __toString)
{
	php_ion_symbol *sym = php_ion_obj(symbol, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(sym);

	ZEND_PARSE_PARAMETERS_NONE();

	if (!sym->value) {
		RETURN_EMPTY_STRING();
	}
	RETURN_STR_COPY(sym->value);
}
ZEND_METHOD(ion_Timestamp, __construct)
{
	php_ion_timestamp *obj = php_ion_obj(timestamp, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_long precision;
	zend_object *precision_obj;
	zend_string *fmt = NULL, *dt = NULL;
	zval *tz = NULL;
	ZEND_PARSE_PARAMETERS_START(1, 4)
		Z_PARAM_OBJ_OF_CLASS_OR_LONG(precision_obj, ce_Timestamp_Precision, precision)
		Z_PARAM_OPTIONAL
		Z_PARAM_STR_OR_NULL(fmt)
		Z_PARAM_STR_OR_NULL(dt)
		Z_PARAM_ZVAL(tz)
	ZEND_PARSE_PARAMETERS_END();

	if (precision_obj) {
		precision = Z_LVAL_P(zend_enum_fetch_case_value(precision_obj));
	}
	php_ion_timestamp_ctor(obj, precision, fmt, dt, tz);
}
ZEND_METHOD(ion_Timestamp, __toString)
{
	php_ion_timestamp *obj = php_ion_obj(timestamp, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	zval fmt;
	ZVAL_NULL(&fmt);
	zend_call_method_with_1_params(&obj->std, obj->std.ce, NULL, "format", return_value,
		zend_read_property(obj->std.ce, &obj->std, ZEND_STRL("format"), 0, &fmt));
}
ZEND_METHOD(ion_Decimal_Context, __construct)
{
	php_ion_decimal_ctx *obj = php_ion_obj(decimal_ctx, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_bool clamp;
	zend_object *o_round = NULL;
	zend_long digits, emax, emin, round;
	ZEND_PARSE_PARAMETERS_START(5, 5)
		Z_PARAM_LONG(digits)
		Z_PARAM_LONG(emax)
		Z_PARAM_LONG(emin)
		Z_PARAM_OBJ_OF_CLASS_OR_LONG(o_round, ce_Decimal_Context_Rounding, round)
		Z_PARAM_BOOL(clamp)
	ZEND_PARSE_PARAMETERS_END();

	if (o_round) {
		round = Z_LVAL_P(zend_enum_fetch_case_value(o_round));
	}
	php_ion_decimal_ctx_init(&obj->ctx, digits, emax, emin, round, clamp);
	php_ion_decimal_ctx_ctor(obj, o_round);
}
static inline void make_decimal_ctx(INTERNAL_FUNCTION_PARAMETERS, int kind)
{
	ZEND_PARSE_PARAMETERS_NONE();

	object_init_ex(return_value, ce_Decimal_Context);
	php_ion_decimal_ctx *obj = php_ion_obj(decimal_ctx, Z_OBJ_P(return_value));
	decContextDefault(&obj->ctx, kind);
	php_ion_decimal_ctx_ctor(obj, NULL);
}
ZEND_METHOD(ion_Decimal_Context, Dec32)
{
	make_decimal_ctx(INTERNAL_FUNCTION_PARAM_PASSTHRU, DEC_INIT_DECIMAL32);
}
ZEND_METHOD(ion_Decimal_Context, Dec64)
{
	make_decimal_ctx(INTERNAL_FUNCTION_PARAM_PASSTHRU, DEC_INIT_DECIMAL64);
}
ZEND_METHOD(ion_Decimal_Context, Dec128)
{
	make_decimal_ctx(INTERNAL_FUNCTION_PARAM_PASSTHRU, DEC_INIT_DECIMAL128);
}
ZEND_METHOD(ion_Decimal_Context, DecMax)
{
	zend_object *o_round = NULL;
	zend_long round = DEC_ROUND_HALF_EVEN;
	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_LONG(o_round, ce_Decimal_Context_Rounding, round)
	ZEND_PARSE_PARAMETERS_END();

	if (o_round) {
		round = Z_LVAL_P(zend_enum_fetch_case_value(o_round));
	}
	object_init_ex(return_value, ce_Decimal_Context);
	php_ion_decimal_ctx *obj = php_ion_obj(decimal_ctx, Z_OBJ_P(return_value));
	php_ion_decimal_ctx_init_max(&obj->ctx, round);
	php_ion_decimal_ctx_ctor(obj, o_round);
}
ZEND_METHOD(ion_Decimal, __construct)
{
	php_ion_decimal *obj = php_ion_obj(decimal, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_long num;
	zend_string *zstr;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR_OR_LONG(zstr, num)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->ctx, ce_Decimal_Context)
	ZEND_PARSE_PARAMETERS_END();

	if (obj->ctx) {
		GC_ADDREF(obj->ctx);
	} else {
		zval zdc;
		object_init_ex(&zdc, ce_Decimal_Context);
		obj->ctx = Z_OBJ(zdc);
		php_ion_decimal_ctx_ctor(php_ion_obj(decimal_ctx, obj->ctx), NULL);
	}

	decContext *ctx = &php_ion_obj(decimal_ctx, obj->ctx)->ctx;

	if (zstr) {
		ION_CHECK(ion_decimal_from_string(&obj->dec, zstr->val, ctx), OBJ_RELEASE(obj->ctx));
	} else {
		php_ion_decimal_from_zend_long(&obj->dec, ctx, num);
	}

	php_ion_decimal_ctor(obj);
	OBJ_RELEASE(obj->ctx);
}
ZEND_METHOD(ion_Decimal, equals)
{
	php_ion_decimal *obj = php_ion_obj(decimal, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_object *dec_obj;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(dec_obj, ce_Decimal)
	ZEND_PARSE_PARAMETERS_END();

	BOOL is = FALSE;
	ION_CHECK(ion_decimal_equals(&obj->dec, &php_ion_obj(decimal, dec_obj)->dec,
		obj->ctx ? &php_ion_obj(decimal_ctx, obj->ctx)->ctx : NULL, &is));
	RETURN_BOOL(is);
}
ZEND_METHOD(ion_Decimal, __toString)
{
	php_ion_decimal *obj = php_ion_obj(decimal, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_STR(php_ion_decimal_to_string(&obj->dec));
}
ZEND_METHOD(ion_Decimal, toInt)
{
	php_ion_decimal *obj = php_ion_obj(decimal, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	zend_long l;
	php_ion_decimal_to_zend_long(&obj->dec, &php_ion_obj(decimal_ctx, obj->ctx)->ctx, &l);
	RETURN_LONG(l);
}
ZEND_METHOD(ion_Decimal, isInt)
{
	php_ion_decimal *obj = php_ion_obj(decimal, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(ion_decimal_is_integer(&obj->dec));
}
ZEND_METHOD(ion_LOB, __construct)
{
	zend_string *value;
	zend_object *type = NULL;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(value)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS(type, ce_Type)
	ZEND_PARSE_PARAMETERS_END();

	if (!type) {
		type = zend_enum_get_case_cstr(ce_Type, "CLob");
	}
	update_property_obj(Z_OBJ_P(ZEND_THIS), ZEND_STRL("type"), type);
	zend_update_property_str(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), ZEND_STRL("value"), value);
}
ZEND_METHOD(ion_Reader_Options, __construct)
{
	php_ion_reader_options *opt = php_ion_obj(reader_options, Z_OBJ_P(ZEND_THIS));
	zend_bool ret_sys_val = false, skip_validation = false;
	zend_long ch_nl = 0xa, max_depth = 10, max_ann, max_ann_buf = 512,
			sym_thr = 0x1000, uval_thr = 0x1000, chunk_thr = 0x1000, alloc_pgsz = 0x1000;

	PTR_CHECK(opt);

	ZEND_PARSE_PARAMETERS_START(0, 13)
		Z_PARAM_OPTIONAL
        // public readonly ?\ion\Catalog $catalog = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(opt->cat, ce_Catalog)
        // public readonly ?\ion\Decimal\Context $decimalContext = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(opt->dec_ctx, ce_Decimal_Context)
        // public readonly ?\Closure $onContextChange = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(opt->cb, zend_ce_closure);
        // public readonly bool $returnSystemValues = false,
		Z_PARAM_BOOL(ret_sys_val)
        // public readonly int $newLine = 0xa,
		Z_PARAM_LONG(ch_nl)
        // public readonly int $maxContainerDepth = 10,
		Z_PARAM_LONG(max_depth)
        // public readonly int $maxAnnotations = 10,
		Z_PARAM_LONG(max_ann)
        // public readonly int $maxAnnotationBuffered = 512,
		Z_PARAM_LONG(max_ann_buf)
        // public readonly int $symbolThreshold = 4096,
		Z_PARAM_LONG(sym_thr)
        // public readonly int $userValueThreshold = 4096,
		Z_PARAM_LONG(uval_thr)
        // public readonly int $chunkThreshold = 4096,
		Z_PARAM_LONG(chunk_thr)
        // public readonly int $allocationPageSize = 4096,
		Z_PARAM_LONG(alloc_pgsz)
        // public readonly bool $skipCharacterValidation = false,
		Z_PARAM_BOOL(skip_validation)
	ZEND_PARSE_PARAMETERS_END();

	opt->opt.context_change_notifier = EMPTY_READER_CHANGE_NOTIFIER;
	if (opt->cb) {
		update_property_obj(&opt->std, ZEND_STRL("onContextChange"), opt->cb);
	}
	if (opt->cat) {
		update_property_obj(&opt->std, ZEND_STRL("catalog"), opt->cat);
		opt->opt.pcatalog = php_ion_obj(catalog, opt->cat)->cat;
	}
	if (opt->dec_ctx) {
		update_property_obj(&opt->std, ZEND_STRL("decimalContext"), opt->dec_ctx);
		opt->opt.decimal_context = &php_ion_obj(decimal_ctx, opt->dec_ctx)->ctx;
	}
	zend_update_property_bool(opt->std.ce, &opt->std, ZEND_STRL("returnSystemValues"),
		opt->opt.return_system_values = ret_sys_val);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("newLine"),
		opt->opt.new_line_char = ch_nl);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("maxContainerDepth"),
		opt->opt.max_container_depth = max_depth);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("maxAnnotationCount"),
		opt->opt.max_annotation_count = max_ann);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("maxAnnotationBuffered"),
		opt->opt.max_annotation_buffered = max_ann_buf);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("symbolThreshold"),
		opt->opt.symbol_threshold = sym_thr);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("userValueThreshold"),
		opt->opt.user_value_threshold = uval_thr);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("chunkThreshold"),
		opt->opt.chunk_threshold = chunk_thr);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("allocationPageSize"),
		opt->opt.allocation_page_size = alloc_pgsz);
	zend_update_property_long(opt->std.ce, &opt->std, ZEND_STRL("skipCharacterValidation"),
		opt->opt.skip_character_validation = skip_validation);
}
ZEND_METHOD(ion_Reader_Reader, hasChildren)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));

	OBJ_CHECK(obj);
	ZEND_PARSE_PARAMETERS_NONE();

	ION_TYPE t;
	ION_CHECK(ion_reader_get_type(obj->reader, &t));
	switch (ION_TYPE_INT(t)) {
		case tid_LIST_INT:
		case tid_SEXP_INT:
		case tid_STRUCT_INT:
			RETVAL_TRUE;
			break;
		default:
			RETVAL_FALSE;
			break;
	}
}
ZEND_METHOD(ion_Reader_Reader, getChildren)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_reader_step_in(obj->reader));

	RETURN_ZVAL(ZEND_THIS, 1, 0);
}
ZEND_METHOD(ion_Reader_Reader, rewind)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_reader_next(obj->reader, &obj->state));
}
ZEND_METHOD(ion_Reader_Reader, next)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	if (obj->state == tid_EOF) {
		SIZE depth = 0;
		ION_CHECK(ion_reader_get_depth(obj->reader, &depth));
		if (depth) {
			ION_CHECK(ion_reader_step_out(obj->reader));
		}
	}
	ION_CHECK(ion_reader_next(obj->reader, &obj->state));
}
ZEND_METHOD(ion_Reader_Reader, valid)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_BOOL(obj->state != tid_none && obj->state != tid_EOF);
}
ZEND_METHOD(ion_Reader_Reader, key)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_IONTYPE(obj->state);
}
ZEND_METHOD(ion_Reader_Reader, current)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_ZVAL(ZEND_THIS, 1, 0);
}
ZEND_METHOD(ion_Reader_Reader, getType)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_TYPE typ;
	ION_CHECK(ion_reader_get_type(obj->reader, &typ));
	RETURN_IONTYPE(typ);
}
ZEND_METHOD(ion_Reader_Reader, hasAnnotations)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	BOOL has = FALSE;
	ION_CHECK(ion_reader_has_any_annotations(obj->reader, &has));
	RETURN_BOOL(has);
}
ZEND_METHOD(ion_Reader_Reader, hasAnnotation)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_string *ann_zstr;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_STR(ann_zstr);
	ZEND_PARSE_PARAMETERS_END();

	ION_STRING ann_istr;
	BOOL has = FALSE;
	ION_CHECK(ion_reader_has_annotation(obj->reader, ion_string_from_zend(&ann_istr, ann_zstr), &has));
	RETURN_BOOL(has);
}
ZEND_METHOD(ion_Reader_Reader, isNull)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	BOOL is = FALSE;
	ION_CHECK(ion_reader_is_null(obj->reader, &is));
	RETURN_BOOL(is);
}
ZEND_METHOD(ion_Reader_Reader, isInStruct)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	BOOL is = FALSE;
	ION_CHECK(ion_reader_is_in_struct(obj->reader, &is));
	RETURN_BOOL(is);
}
ZEND_METHOD(ion_Reader_Reader, getFieldName)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_STRING name;
	ION_CHECK(ion_reader_get_field_name(obj->reader, &name));
	RETURN_STRINGL((char *) name.value, name.length);
}
ZEND_METHOD(ion_Reader_Reader, getFieldNameSymbol)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_SYMBOL *sym_ptr;
	ION_CHECK(ion_reader_get_field_name_symbol(obj->reader, &sym_ptr));

	php_ion_symbol_zval(sym_ptr, return_value);
}
ZEND_METHOD(ion_Reader_Reader, getAnnotations)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	int32_t count, max;
	if (obj->opt) {
		max = php_ion_obj(reader_options, obj->opt)->opt.max_annotation_count;
	} else {
		max = 10;
	}
	ION_STRING *ptr = ecalloc(sizeof(*ptr), max);
	iERR err = ion_reader_get_annotations(obj->reader, ptr, max, &count);
	if (!err) {
		array_init_size(return_value, count);
		for (int32_t i = 0; i < count; ++i) {
			add_next_index_str(return_value, zend_string_from_ion(&ptr[i]));
		}
	}
	efree(ptr);
	ION_CHECK(err);
}
ZEND_METHOD(ion_Reader_Reader, getAnnotationSymbols)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	int32_t count, max = php_ion_obj(reader_options, obj->opt)->opt.max_annotation_count;
	ION_SYMBOL *ptr = ecalloc(sizeof(*ptr), max);
	iERR err = ion_reader_get_annotation_symbols(obj->reader, ptr, max, &count);
	if (!err) {
		array_init_size(return_value, count);
		for (int32_t i = 0; i < count; ++i) {
			zval zsym;
			php_ion_symbol_zval(&ptr[i], &zsym);
			add_next_index_zval(return_value, &zsym);
		}
	}
	efree(ptr);
	ION_CHECK(err);
}
ZEND_METHOD(ion_Reader_Reader, countAnnotations)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE sz = 0;
	ION_CHECK(ion_reader_get_annotation_count(obj->reader, &sz));
	RETURN_LONG(sz);
}
ZEND_METHOD(ion_Reader_Reader, getAnnotation)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_long idx;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(idx);
	ZEND_PARSE_PARAMETERS_END();

	ION_STRING ann;
	ION_CHECK(ion_reader_get_an_annotation(obj->reader, idx, &ann));
	RETURN_STRINGL((char *) ann.value, ann.length);
}
ZEND_METHOD(ion_Reader_Reader, getAnnotationSymbol)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_long idx;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(idx);
	ZEND_PARSE_PARAMETERS_END();

	ION_SYMBOL sym;
	ION_CHECK(ion_reader_get_an_annotation_symbol(obj->reader, idx, &sym));
	php_ion_symbol_zval(&sym, return_value);
}
ZEND_METHOD(ion_Reader_Reader, readNull)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_TYPE typ;
	ION_CHECK(ion_reader_read_null(obj->reader, &typ));
	RETURN_OBJ_COPY(php_ion_type_fetch(typ));
}
ZEND_METHOD(ion_Reader_Reader, readBool)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	BOOL b;
	ION_CHECK(ion_reader_read_bool(obj->reader, &b));
	RETURN_BOOL(b);
}
ZEND_METHOD(ion_Reader_Reader, readInt)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	php_ion_reader_read_int(obj->reader, return_value);
}
ZEND_METHOD(ion_Reader_Reader, readFloat)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_reader_read_double(obj->reader, &Z_DVAL_P(return_value)));
}
ZEND_METHOD(ion_Reader_Reader, readDecimal)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	object_init_ex(return_value, ce_Decimal);
	php_ion_decimal *dec = php_ion_obj(decimal, Z_OBJ_P(return_value));
	ION_CHECK(ion_reader_read_ion_decimal(obj->reader, &dec->dec));
	php_ion_decimal_ctor(dec);
}
ZEND_METHOD(ion_Reader_Reader, readTimestamp)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	php_ion_reader_read_timestamp(obj->reader, obj->opt ? &php_ion_obj(reader_options, obj->opt)->opt : NULL, return_value);
}
ZEND_METHOD(ion_Reader_Reader, readSymbol)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_SYMBOL sym;
	ION_CHECK(ion_reader_read_ion_symbol(obj->reader, &sym));
	php_ion_symbol_zval(&sym, return_value);
}
ZEND_METHOD(ion_Reader_Reader, readString)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_STRING str;
	ION_CHECK(ion_reader_read_string(obj->reader, &str));
	RETURN_STRINGL((char *) str.value, str.length);
}

typedef iERR (*read_part_fn)(ION_READER *, BYTE *, SIZE, SIZE *);
static void read_part(INTERNAL_FUNCTION_PARAMETERS, read_part_fn fn)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zval *ref;
	zend_string *zstr;
	zend_long len = 0x1000;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(ref)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(len)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DEREF(ref);

	if (Z_TYPE_P(ref) == IS_STRING && Z_STRLEN_P(ref) == len) {
		/* reuse */
		zstr = Z_STR_P(ref);
	} else {
		zval_dtor(ref);
		zstr = zend_string_alloc(len, 0);
	}

	SIZE read = 0;
	ION_CHECK(fn(obj->reader, (BYTE *) zstr->val, zstr->len, &read), goto fail);
	if (read > 0) {
		if (read < zstr->len) {
			zstr = zend_string_truncate(zstr, read, 0);
		}
		ZVAL_STR(ref, zstr);
		RETURN_TRUE;
	}
fail:
	if (zstr != Z_STR_P(ref)) {
		zend_string_release(zstr);
	}
	ZVAL_EMPTY_STRING(ref);
	RETURN_FALSE;
}
ZEND_METHOD(ion_Reader_Reader, readStringPart)
{
	read_part(INTERNAL_FUNCTION_PARAM_PASSTHRU, ion_reader_read_partial_string);
}
ZEND_METHOD(ion_Reader_Reader, readLob)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	php_ion_reader_read_lob(obj->reader, return_value);
}
ZEND_METHOD(ion_Reader_Reader, readLobPart)
{
	read_part(INTERNAL_FUNCTION_PARAM_PASSTHRU, ion_reader_read_lob_partial_bytes);
}
ZEND_METHOD(ion_Reader_Reader, getPosition)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	int64_t bytes = 0;
	int32_t dummy;
	ION_CHECK(ion_reader_get_position(obj->reader, &bytes, &dummy, &dummy));
	RETURN_LONG(bytes);
}
ZEND_METHOD(ion_Reader_Reader, getDepth)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE depth = 0;
	ION_CHECK(ion_reader_get_depth(obj->reader, &depth));
	RETURN_LONG(depth);
}
ZEND_METHOD(ion_Reader_Reader, seek)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_long off, len = -1;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_LONG(off)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(len)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_reader_seek(obj->reader, off, len));
}
ZEND_METHOD(ion_Reader_Reader, getValueOffset)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	POSITION off = 0;
	ION_CHECK(ion_reader_get_value_offset(obj->reader, &off));
	RETURN_LONG(off);
}
ZEND_METHOD(ion_Reader_Reader, getValueLength)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE len = 0;
	ION_CHECK(ion_reader_get_value_length(obj->reader, &len));
	RETURN_LONG(len);
}
ZEND_METHOD(ion_Reader_Buffer_Reader, __construct)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_string *zstr;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(zstr)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Reader_Options)
	ZEND_PARSE_PARAMETERS_END();

	obj->type = BUFFER_READER;
	obj->buffer = zend_string_copy(zstr);

	php_ion_reader_ctor(obj);
}
ZEND_METHOD(ion_Reader_Buffer_Reader, getBuffer)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_STR_COPY(obj->buffer);
}

ZEND_METHOD(ion_Reader_Stream_Reader, __construct)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zval *zstream;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_RESOURCE(zstream)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Reader_Options)
	ZEND_PARSE_PARAMETERS_END();

	obj->type = STREAM_READER;
	php_stream_from_zval_no_verify(obj->stream.ptr, zstream);

	php_ion_reader_ctor(obj);
}
ZEND_METHOD(ion_Reader_Stream_Reader, getStream)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();
	PTR_CHECK(obj->stream.ptr);

	GC_ADDREF(obj->stream.ptr->res);
	RETURN_RES(obj->stream.ptr->res);
}
ZEND_METHOD(ion_Reader_Stream_Reader, resetStream)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zval *zstream;
	ZEND_PARSE_PARAMETERS_START(1, 1);
		Z_PARAM_RESOURCE(zstream);
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_reader_reset_stream(&obj->reader, obj, php_ion_reader_stream_handler));

	if (obj->stream.ptr) {
		zend_list_delete(obj->stream.ptr->res);
	}
	php_stream_from_zval_no_verify(obj->stream.ptr, zstream);
	PTR_CHECK(obj->stream.ptr);
	Z_ADDREF_P(zstream);
}
ZEND_METHOD(ion_Reader_Stream_Reader, resetStreamWithLength)
{
	php_ion_reader *obj = php_ion_obj(reader, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zval *zstream;
	zend_long length;
	ZEND_PARSE_PARAMETERS_START(1, 2);
		Z_PARAM_RESOURCE(zstream);
		Z_PARAM_LONG(length)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_reader_reset_stream_with_length(&obj->reader, obj, php_ion_reader_stream_handler, length));

	if (obj->stream.ptr) {
		zend_list_delete(obj->stream.ptr->res);
	}
	php_stream_from_zval_no_verify(obj->stream.ptr, zstream);
	PTR_CHECK(obj->stream.ptr);
	Z_ADDREF_P(zstream);
}
ZEND_METHOD(ion_Writer_Options, __construct)
{
	php_ion_writer_options *obj = php_ion_obj(writer_options, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zend_bool binary = false, compact_floats = false, escape = false, pretty = false,
			tabs = true, small_cntr_inl = true, suppress_sys = false, flush = false;
	zend_long indent = 2, max_depth = 10, max_ann = 10, temp = 0x400, alloc = 0x1000;
	ZEND_PARSE_PARAMETERS_START(0, 16)
		Z_PARAM_OPTIONAL
		//public readonly ?\ion\Catalog $catalog = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->cat, ce_Catalog)
		//public readonly ?\ion\Collection $encodingSymbolTable = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->col, ce_Collection)
		//public readonly ?\ion\Decimal\Context $decimalContext = null,
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->dec_ctx, ce_Decimal_Context)
		//public readonly bool $outputBinary = false,
		Z_PARAM_BOOL(binary)
		//public readonly bool $compactFloats = false,
		Z_PARAM_BOOL(compact_floats)
		//public readonly bool $escapeNonAscii = false,
		Z_PARAM_BOOL(escape)
		//public readonly bool $prettyPrint = false,
		Z_PARAM_BOOL(pretty)
		//public readonly bool $indentTabs = true,
		Z_PARAM_BOOL(tabs)
		//public readonly int $indentSize = 2,
		Z_PARAM_LONG(indent)
		//public readonly bool $smallContainersInline = true,
		Z_PARAM_BOOL(small_cntr_inl)
		//public readonly bool $suppressSystemValues = false,
		Z_PARAM_BOOL(suppress_sys)
		//public readonly bool $flushEveryValue = false,
		Z_PARAM_BOOL(flush)
		//public readonly int $maxContainerDepth = 10,
		Z_PARAM_LONG(max_depth)
		//public readonly int $maxAnnotations = 10,
		Z_PARAM_LONG(max_ann)
		//public readonly int $tempBufferSize = 0x400,
		Z_PARAM_LONG(temp)
		//public readonly int $allocationPageSize = 0x1000,
		Z_PARAM_LONG(alloc)
	ZEND_PARSE_PARAMETERS_END();

	if (obj->cat) {
		update_property_obj(&obj->std, ZEND_STRL("catalog"), obj->cat);
		obj->opt.pcatalog = php_ion_obj(catalog, obj->cat)->cat;
	}
	if (obj->col) {
		// TODO
	}
	if (obj->dec_ctx) {
		update_property_obj(&obj->std, ZEND_STRL("decimalContext"), obj->dec_ctx);
		obj->opt.decimal_context = &php_ion_obj(decimal_ctx, obj->dec_ctx)->ctx;
	}
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("outputBinary"),
			obj->opt.output_as_binary = binary);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("comactFloats"),
			obj->opt.compact_floats = compact_floats);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("excapeNonAscii"),
			obj->opt.escape_all_non_ascii = escape);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("prettyPrint"),
			obj->opt.pretty_print = pretty);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("indentTabs"),
			obj->opt.indent_with_tabs = tabs);
	zend_update_property_long(obj->std.ce, &obj->std, ZEND_STRL("indentSize"),
			obj->opt.indent_size = indent);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("smallContainersInline"),
			obj->opt.small_containers_in_line = small_cntr_inl);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("suppressSystemValues"),
			obj->opt.supress_system_values = suppress_sys);
	zend_update_property_bool(obj->std.ce, &obj->std, ZEND_STRL("flushEveryValue"),
			obj->opt.flush_every_value = flush);
	zend_update_property_long(obj->std.ce, &obj->std, ZEND_STRL("maxContainerDepth"),
			obj->opt.max_container_depth = max_depth);
	zend_update_property_long(obj->std.ce, &obj->std, ZEND_STRL("maxAnnotations"),
			obj->opt.max_annotation_count = max_ann);
	zend_update_property_long(obj->std.ce, &obj->std, ZEND_STRL("tempBufferSize"),
			obj->opt.temp_buffer_size = temp);
	zend_update_property_long(obj->std.ce, &obj->std, ZEND_STRL("allocationPageSize"),
			obj->opt.allocation_page_size = alloc);
}
ZEND_METHOD(ion_Writer_Writer, writeNull)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_writer_write_null(obj->writer));
}
ZEND_METHOD(ion_Writer_Writer, writeTypedNull)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *typ_obj;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(typ_obj, ce_Type)
	ZEND_PARSE_PARAMETERS_END();

	php_ion_type *typ = php_ion_obj(type, typ_obj);
	OBJ_CHECK(typ);
	ION_CHECK(ion_writer_write_typed_null(obj->writer, php_ion_obj(type, typ)->typ));
}
ZEND_METHOD(ion_Writer_Writer, writeBool)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_bool b;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_BOOL(b)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_writer_write_bool(obj->writer, b));
}
ZEND_METHOD(ion_Writer_Writer, writeInt)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_long l;
	zend_string *s;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR_OR_LONG(s, l)
	ZEND_PARSE_PARAMETERS_END();

	if (s) {
		ION_INT *i;
		ION_CHECK(ion_int_alloc(obj->writer, &i));
		ION_CHECK(ion_int_from_chars(i, s->val, s->len));
		ION_CHECK(ion_writer_write_ion_int(obj->writer, i));
		ion_int_free(i);
	} else {
		ION_CHECK(ion_writer_write_int64(obj->writer, l));
	}
}
ZEND_METHOD(ion_Writer_Writer, writeFloat)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	double d;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_DOUBLE(d)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_writer_write_double(obj->writer, d));
}
ZEND_METHOD(ion_Writer_Writer, writeDecimal)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *dec_obj;
	zend_string *dec_str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR(dec_obj, ce_Decimal, dec_str)
	ZEND_PARSE_PARAMETERS_END();

	if (dec_str) {
		ION_STRING s;
		ION_CHECK(ion_writer_write_string(obj->writer, ion_string_from_zend(&s, dec_str)));
	} else {
		php_ion_decimal *dec = php_ion_obj(decimal, dec_obj);
		ION_CHECK(ion_writer_write_ion_decimal(obj->writer, &dec->dec));
	}
}
ZEND_METHOD(ion_Writer_Writer, writeTimestamp)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *ts_obj;
	zend_string *ts_str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR(ts_obj, ce_Timestamp, ts_str)
	ZEND_PARSE_PARAMETERS_END();

	decContext *ctx = NULL;
	if (obj->opt) {
		ctx = php_ion_obj(reader_options, obj->opt)->opt.decimal_context;
	}

	ION_TIMESTAMP tmp = {0};
	if (ts_str) {
		SIZE used;
		ION_CHECK(ion_timestamp_parse(&tmp, ts_str->val, ts_str->len, &used, ctx));
	} else {
		php_ion_timestamp *ts = php_ion_obj(timestamp, ts_obj);
		OBJ_CHECK(ts);
		ion_timestamp_from_php(&tmp, ts, ctx);
	}
	ION_CHECK(ion_writer_write_timestamp(obj->writer, &tmp));
}
ZEND_METHOD(ion_Writer_Writer, writeSymbol)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *sym_obj;
	zend_string *sym_str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR(sym_obj, ce_Symbol, sym_str);
	ZEND_PARSE_PARAMETERS_END();

	if (sym_str) {
		ION_STRING is;
		ION_CHECK(ion_writer_write_symbol(obj->writer, ion_string_from_zend(&is, sym_str)));
	} else {
		php_ion_symbol *sym = php_ion_obj(symbol, sym_obj);
		PTR_CHECK(sym);
		ION_CHECK(ion_writer_write_ion_symbol(obj->writer, &sym->sym));
	}
}
ZEND_METHOD(ion_Writer_Writer, writeString)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_string *str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ION_STRING is;
	ION_CHECK(ion_writer_write_string(obj->writer, ion_string_from_zend(&is, str)));
}
ZEND_METHOD(ion_Writer_Writer, writeCLob)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_string *str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_writer_write_clob(obj->writer, (BYTE *) str->val, str->len));
}
ZEND_METHOD(ion_Writer_Writer, writeBLob)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_string *str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_writer_write_blob(obj->writer, (BYTE *) str->val, str->len));
}
ZEND_METHOD(ion_Writer_Writer, startLob)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *typ_obj;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(typ_obj, ce_Type)
	ZEND_PARSE_PARAMETERS_END();

	php_ion_type *typ = php_ion_obj(type, typ_obj);
	OBJ_CHECK(typ);
	ION_CHECK(ion_writer_start_lob(obj->writer, php_ion_obj(type, typ)->typ));
}
ZEND_METHOD(ion_Writer_Writer, appendLob)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_string *str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ION_CHECK(ion_writer_append_lob(obj->writer, (BYTE *) str->val, str->len));
}
ZEND_METHOD(ion_Writer_Writer, finishLob)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_writer_finish_lob(obj->writer));
}
ZEND_METHOD(ion_Writer_Writer, startContainer)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *typ_obj;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS(typ_obj, ce_Type)
	ZEND_PARSE_PARAMETERS_END();

	php_ion_type *typ = php_ion_obj(type, typ_obj);
	OBJ_CHECK(typ);
	ION_CHECK(ion_writer_start_container(obj->writer, php_ion_obj(type, typ)->typ));
}
ZEND_METHOD(ion_Writer_Writer, finishContainer)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	ION_CHECK(ion_writer_finish_container(obj->writer));
}
ZEND_METHOD(ion_Writer_Writer, writeFieldName)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zend_object *sym_obj;
	zend_string *sym_str;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJ_OF_CLASS_OR_STR(sym_obj, ce_Symbol, sym_str);
	ZEND_PARSE_PARAMETERS_END();

	if (sym_str) {
		ION_STRING is;
		ION_CHECK(ion_writer_write_field_name(obj->writer, ion_string_from_zend(&is, sym_str)));
	} else {
		php_ion_symbol *sym = php_ion_obj(symbol, sym_obj);
		PTR_CHECK(sym);
		ION_CHECK(ion_writer_write_field_name_symbol(obj->writer, &sym->sym));
	}
}
ZEND_METHOD(ion_Writer_Writer, writeAnnotation)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	zval *args;
	unsigned argc;
	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_VARIADIC('+', args, argc);
	ZEND_PARSE_PARAMETERS_END();

	for (unsigned i = 0; i < argc; ++i) {
		switch (Z_TYPE(args[i])) {
		case IS_STRING:
			ION_STRING is;
			ION_CHECK(ion_writer_add_annotation(obj->writer, ion_string_from_zend(&is, Z_STR(args[i]))));
			break;

		case IS_OBJECT:
			ION_CHECK(ion_writer_add_annotation_symbol(obj->writer, &php_ion_obj(symbol, Z_OBJ(args[i]))->sym));
			break;
		}
	}
}
ZEND_METHOD(ion_Writer_Writer, getDepth)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE depth;
	ION_CHECK(ion_writer_get_depth(obj->writer, &depth));
	RETURN_LONG(depth);
}
ZEND_METHOD(ion_Writer_Writer, flush)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE flushed;
	ION_CHECK(ion_writer_flush(obj->writer, &flushed));
	if (obj->type == BUFFER_WRITER) {
		smart_str_0(&obj->buffer.str);
	}
	RETURN_LONG(flushed);
}
ZEND_METHOD(ion_Writer_Writer, finish)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	SIZE flushed;
	ION_CHECK(ion_writer_finish(obj->writer, &flushed));
	if (obj->type == BUFFER_WRITER) {
		smart_str_0(&obj->buffer.str);
	}
	RETURN_LONG(flushed);
}
ZEND_METHOD(ion_Writer_Writer, writeOne)
{
}
ZEND_METHOD(ion_Writer_Writer, writeAll)
{
}
ZEND_METHOD(ion_Writer_Buffer_Writer, __construct)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zval *ref;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(ref)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Writer_Options)
	ZEND_PARSE_PARAMETERS_END();

	obj->type = BUFFER_WRITER;
	ZVAL_COPY(&obj->buffer.val, ref);
	zval_dtor(Z_REFVAL_P(ref));

	php_ion_writer_ctor(obj);
}
ZEND_METHOD(ion_Writer_Buffer_Writer, getBuffer)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();

	RETVAL_STR(zend_string_dup(obj->buffer.str.s, 0));
}
ZEND_METHOD(ion_Writer_Stream_Writer, __construct)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	zval *zstream;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_RESOURCE(zstream)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Writer_Options)
	ZEND_PARSE_PARAMETERS_END();

	obj->type = STREAM_WRITER;
	php_stream_from_zval_no_verify(obj->stream.ptr, zstream);

	php_ion_writer_ctor(obj);
}
ZEND_METHOD(ion_Writer_Stream_Writer, getStream)
{
	php_ion_writer *obj = php_ion_obj(writer, Z_OBJ_P(ZEND_THIS));
	OBJ_CHECK(obj);

	ZEND_PARSE_PARAMETERS_NONE();
	PTR_CHECK(obj->stream.ptr);

	GC_ADDREF(obj->stream.ptr->res);
	RETURN_RES(obj->stream.ptr->res);
}

ZEND_METHOD(ion_Serializer_PHP, __construct)
{
	php_ion_serializer_php *obj = php_ion_obj(serializer_php, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	obj->serializer.call_magic = true;

	ZEND_PARSE_PARAMETERS_START(0, 3)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Writer_Options)
		Z_PARAM_BOOL(obj->serializer.multi_seq)
		Z_PARAM_BOOL(obj->serializer.call_magic)
		Z_PARAM_STR_OR_NULL(obj->serializer.call_custom)
	ZEND_PARSE_PARAMETERS_END();

	php_ion_serializer_php_ctor(obj);
}
ZEND_METHOD(ion_Serializer_PHP, __invoke)
{
	zend_object *obj = Z_OBJ_P(ZEND_THIS);

	zval *data;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(data)
	ZEND_PARSE_PARAMETERS_END();

	if (obj->ce == ce_Serializer_PHP) {
		// default, fast path
		php_ion_serialize(&php_ion_obj(serializer_php, obj)->serializer, data, return_value);
	} else {
		zend_call_method_with_1_params(obj, obj->ce, NULL /* TODO */, "serialize", return_value, data);
	}
}
ZEND_FUNCTION(ion_serialize)
{
	zval *data;
	zend_object *zo_ser = NULL;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(data)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(zo_ser, ce_Serializer)
	ZEND_PARSE_PARAMETERS_END();

	if (!zo_ser || zo_ser->ce == ce_Serializer_PHP) {
		// default, fast path
		php_ion_serializer *ser = zo_ser ? &php_ion_obj(serializer_php, zo_ser)->serializer : NULL;
		php_ion_serialize(ser, data, return_value);
	} else {
		zend_call_method_with_1_params(zo_ser, NULL, NULL, "__invoke", return_value, data);
	}
}
ZEND_METHOD(ion_Serializer_PHP, serialize)
{
	//zend_object *obj = Z_OBJ_P(ZEND_THIS);

	zval *data;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(data)
	ZEND_PARSE_PARAMETERS_END();

	// TODO
	zend_throw_exception_ex(spl_ce_BadMethodCallException, 0, "Not implemented");
}

ZEND_METHOD(ion_Unserializer_PHP, __construct)
{
	php_ion_unserializer_php *obj = php_ion_obj(unserializer_php, Z_OBJ_P(ZEND_THIS));
	PTR_CHECK(obj);

	obj->unserializer.call_magic = true;

	ZEND_PARSE_PARAMETERS_START(0, 3)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(obj->opt, ce_Reader_Options)
		Z_PARAM_BOOL(obj->unserializer.multi_seq)
		Z_PARAM_BOOL(obj->unserializer.call_magic)
		Z_PARAM_STR_OR_NULL(obj->unserializer.call_custom)
	ZEND_PARSE_PARAMETERS_END();

	php_ion_unserializer_php_ctor(obj);
}
ZEND_METHOD(ion_Unserializer_PHP, __invoke)
{
	zend_object *obj = Z_OBJ_P(ZEND_THIS);

	zval *data;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(data)
	ZEND_PARSE_PARAMETERS_END();

	if (obj->ce == ce_Unserializer_PHP) {
		php_ion_unserialize(&php_ion_obj(unserializer_php, obj)->unserializer, data, return_value);
	} else {
		zend_call_method_with_1_params(obj, obj->ce, NULL /* TODO */, "unserialize", return_value, data);
	}
}
ZEND_FUNCTION(ion_unserialize)
{
	zval *data;
	zend_object *zo_ser = NULL;
	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(data)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(zo_ser, ce_Unserializer)
	ZEND_PARSE_PARAMETERS_END();

	if (!zo_ser || zo_ser->ce == ce_Unserializer_PHP) {
		// default, fast path
		php_ion_unserializer *ser = zo_ser ? &php_ion_obj(unserializer_php, zo_ser)->unserializer : NULL;
		php_ion_unserialize(ser, data, return_value);
	} else {
		zend_call_method_with_1_params(zo_ser, NULL, NULL, "__invoke", return_value, data);
	}
}
ZEND_METHOD(ion_Unserializer_PHP, unserialize)
{
	//zend_object *obj = Z_OBJ_P(ZEND_THIS);

	zval *data;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(data)
	ZEND_PARSE_PARAMETERS_END();

	// TODO
	zend_throw_exception_ex(spl_ce_BadMethodCallException, 0, "Not implemented");
}

PHP_RINIT_FUNCTION(ion)
{
#if defined(ZTS) && defined(COMPILE_DL_ION)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	php_ion_globals_serializer_init();
	php_ion_globals_unserializer_init();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(ion)
{
	php_ion_globals_serializer_dtor();
	php_ion_globals_unserializer_dtor();
	return SUCCESS;
}

PHP_MINIT_FUNCTION(ion)
{
	// true globals
	php_ion_decimal_from_zend_long(&g_ion_dec_zend_max, &g_dec_ctx, ZEND_LONG_MAX);
	php_ion_decimal_from_zend_long(&g_ion_dec_zend_min, &g_dec_ctx, ZEND_LONG_MIN);

	// Annotation
	ce_Annotation = register_class_ion_Annotation();

	// Catalog
	php_ion_register(catalog, Catalog);

	// Collection
	ce_Collection = register_class_ion_Collection();

	// Decimal
	php_ion_register(decimal, Decimal);
	php_ion_register(decimal_ctx, Decimal_Context);
	ce_Decimal_Context_Rounding = register_class_ion_Decimal_Context_Rounding();

	// LOB
	ce_LOB = register_class_ion_LOB();

	// Reader
	ce_Reader = register_class_ion_Reader(spl_ce_RecursiveIterator);
	php_ion_register(reader_options, Reader_Options);
	php_ion_register(reader, Reader_Reader, ce_Reader);
	ce_Reader_Buffer = register_class_ion_Reader_Buffer(ce_Reader);
	ce_Reader_Buffer_Reader = register_class_ion_Reader_Buffer_Reader(ce_Reader_Reader, ce_Reader_Buffer);
	ce_Reader_Stream = register_class_ion_Reader_Stream(ce_Reader);
	ce_Reader_Stream_Reader = register_class_ion_Reader_Stream_Reader(ce_Reader_Reader, ce_Reader_Stream);

	// Serializer
	ce_Serializer = register_class_ion_Serializer();
	php_ion_register(serializer_php, Serializer_PHP, ce_Serializer);

	// Symbol
	php_ion_register(symbol, Symbol);
	oh_Symbol.compare = php_ion_symbol_zval_compare;
	php_ion_register(symbol_iloc, Symbol_ImportLocation);
	php_ion_register(symbol_table, Symbol_Table);
	ce_Symbol_System = register_class_ion_Symbol_System();
	ce_Symbol_System_SID = register_class_ion_Symbol_System_SID();

	// Timestamp
	ce_Timestamp = register_class_ion_Timestamp(php_date_get_date_ce());
	ce_Timestamp_Precision = register_class_ion_Timestamp_Precision();

	// Type
	php_ion_register(type, Type);

	// Writer
	ce_Writer = register_class_ion_Writer();
	php_ion_register(writer_options, Writer_Options);
	php_ion_register(writer, Writer_Writer, ce_Writer);
	ce_Writer_Buffer = register_class_ion_Writer_Buffer(ce_Writer);
	ce_Writer_Buffer_Writer = register_class_ion_Writer_Buffer_Writer(ce_Writer_Writer, ce_Writer_Buffer);
	ce_Writer_Stream = register_class_ion_Writer_Stream(ce_Writer);
	ce_Writer_Stream_Writer = register_class_ion_Writer_Stream_Writer(ce_Writer_Writer, ce_Writer_Stream);

	// Unserializer
	ce_Unserializer = register_class_ion_Unserializer();
	php_ion_register(unserializer_php, Unserializer_PHP, ce_Unserializer);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(ion)
{
	return SUCCESS;
}
PHP_MINFO_FUNCTION(ion)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "ion support", "enabled");
	php_info_print_table_end();
}

PHP_GINIT_FUNCTION(ion)
{
	memset(ion_globals, 0, sizeof(*ion_globals));

	php_ion_decimal_ctx_init_max(&ion_globals->decimal_ctx, DEC_ROUND_HALF_EVEN);
}
PHP_GSHUTDOWN_FUNCTION(ion)
{
}

static zend_module_dep ion_module_deps[] = {
	ZEND_MOD_REQUIRED("date")
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_END
};

zend_module_entry ion_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	ion_module_deps,
	"ion",					/* Extension name */
	ext_functions,			/* zend_function_entry */
	PHP_MINIT(ion),			/* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(ion),		/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(ion),			/* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(ion),		/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(ion),			/* PHP_MINFO - Module info */
	PHP_ION_VERSION,		/* Version */
	ZEND_MODULE_GLOBALS(ion),
	PHP_GINIT(ion),			/* PHP_GINIT */
	PHP_GSHUTDOWN(ion),		/* PHP_GSHUTDOWN */
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_ION
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(ion)
#endif
