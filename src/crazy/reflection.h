/**
 * @file reflection.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_REFLECTION_H____
#define ____CRAZY_REFLECTION_H____

/******************************************/
/* arg list expand macro, now support 120 args */
#define MAKE_ARG_LIST_1(op, arg, ...)   op(arg)
#define MAKE_ARG_LIST_2(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_1(op, __VA_ARGS__))
#define MAKE_ARG_LIST_3(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_2(op, __VA_ARGS__))
#define MAKE_ARG_LIST_4(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_3(op, __VA_ARGS__))
#define MAKE_ARG_LIST_5(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_4(op, __VA_ARGS__))
#define MAKE_ARG_LIST_6(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_5(op, __VA_ARGS__))
#define MAKE_ARG_LIST_7(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_6(op, __VA_ARGS__))
#define MAKE_ARG_LIST_8(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_7(op, __VA_ARGS__))
#define MAKE_ARG_LIST_9(op, arg, ...)   op(arg), MARCO_EXPAND(MAKE_ARG_LIST_8(op, __VA_ARGS__))
#define MAKE_ARG_LIST_10(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_9(op, __VA_ARGS__))
#define MAKE_ARG_LIST_11(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_10(op, __VA_ARGS__))
#define MAKE_ARG_LIST_12(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_11(op, __VA_ARGS__))
#define MAKE_ARG_LIST_13(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_12(op, __VA_ARGS__))
#define MAKE_ARG_LIST_14(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_13(op, __VA_ARGS__))
#define MAKE_ARG_LIST_15(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_14(op, __VA_ARGS__))
#define MAKE_ARG_LIST_16(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_15(op, __VA_ARGS__))
#define MAKE_ARG_LIST_17(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_16(op, __VA_ARGS__))
#define MAKE_ARG_LIST_18(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_17(op, __VA_ARGS__))
#define MAKE_ARG_LIST_19(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_18(op, __VA_ARGS__))
#define MAKE_ARG_LIST_20(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_19(op, __VA_ARGS__))
#define MAKE_ARG_LIST_21(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_20(op, __VA_ARGS__))
#define MAKE_ARG_LIST_22(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_21(op, __VA_ARGS__))
#define MAKE_ARG_LIST_23(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_22(op, __VA_ARGS__))
#define MAKE_ARG_LIST_24(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_23(op, __VA_ARGS__))
#define MAKE_ARG_LIST_25(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_24(op, __VA_ARGS__))
#define MAKE_ARG_LIST_26(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_25(op, __VA_ARGS__))
#define MAKE_ARG_LIST_27(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_26(op, __VA_ARGS__))
#define MAKE_ARG_LIST_28(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_27(op, __VA_ARGS__))
#define MAKE_ARG_LIST_29(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_28(op, __VA_ARGS__))
#define MAKE_ARG_LIST_30(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_29(op, __VA_ARGS__))
#define MAKE_ARG_LIST_31(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_30(op, __VA_ARGS__))
#define MAKE_ARG_LIST_32(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_31(op, __VA_ARGS__))
#define MAKE_ARG_LIST_33(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_32(op, __VA_ARGS__))
#define MAKE_ARG_LIST_34(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_33(op, __VA_ARGS__))
#define MAKE_ARG_LIST_35(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_34(op, __VA_ARGS__))
#define MAKE_ARG_LIST_36(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_35(op, __VA_ARGS__))
#define MAKE_ARG_LIST_37(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_36(op, __VA_ARGS__))
#define MAKE_ARG_LIST_38(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_37(op, __VA_ARGS__))
#define MAKE_ARG_LIST_39(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_38(op, __VA_ARGS__))
#define MAKE_ARG_LIST_40(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_39(op, __VA_ARGS__))
#define MAKE_ARG_LIST_41(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_40(op, __VA_ARGS__))
#define MAKE_ARG_LIST_42(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_41(op, __VA_ARGS__))
#define MAKE_ARG_LIST_43(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_42(op, __VA_ARGS__))
#define MAKE_ARG_LIST_44(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_43(op, __VA_ARGS__))
#define MAKE_ARG_LIST_45(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_44(op, __VA_ARGS__))
#define MAKE_ARG_LIST_46(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_45(op, __VA_ARGS__))
#define MAKE_ARG_LIST_47(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_46(op, __VA_ARGS__))
#define MAKE_ARG_LIST_48(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_47(op, __VA_ARGS__))
#define MAKE_ARG_LIST_49(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_48(op, __VA_ARGS__))
#define MAKE_ARG_LIST_50(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_49(op, __VA_ARGS__))
#define MAKE_ARG_LIST_51(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_50(op, __VA_ARGS__))
#define MAKE_ARG_LIST_52(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_51(op, __VA_ARGS__))
#define MAKE_ARG_LIST_53(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_52(op, __VA_ARGS__))
#define MAKE_ARG_LIST_54(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_53(op, __VA_ARGS__))
#define MAKE_ARG_LIST_55(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_54(op, __VA_ARGS__))
#define MAKE_ARG_LIST_56(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_55(op, __VA_ARGS__))
#define MAKE_ARG_LIST_57(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_56(op, __VA_ARGS__))
#define MAKE_ARG_LIST_58(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_57(op, __VA_ARGS__))
#define MAKE_ARG_LIST_59(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_58(op, __VA_ARGS__))
#define MAKE_ARG_LIST_60(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_59(op, __VA_ARGS__))
#define MAKE_ARG_LIST_61(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_60(op, __VA_ARGS__))
#define MAKE_ARG_LIST_62(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_61(op, __VA_ARGS__))
#define MAKE_ARG_LIST_63(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_62(op, __VA_ARGS__))
#define MAKE_ARG_LIST_64(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_63(op, __VA_ARGS__))
#define MAKE_ARG_LIST_65(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_64(op, __VA_ARGS__))
#define MAKE_ARG_LIST_66(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_65(op, __VA_ARGS__))
#define MAKE_ARG_LIST_67(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_66(op, __VA_ARGS__))
#define MAKE_ARG_LIST_68(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_67(op, __VA_ARGS__))
#define MAKE_ARG_LIST_69(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_68(op, __VA_ARGS__))
#define MAKE_ARG_LIST_70(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_69(op, __VA_ARGS__))
#define MAKE_ARG_LIST_71(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_70(op, __VA_ARGS__))
#define MAKE_ARG_LIST_72(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_71(op, __VA_ARGS__))
#define MAKE_ARG_LIST_73(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_72(op, __VA_ARGS__))
#define MAKE_ARG_LIST_74(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_73(op, __VA_ARGS__))
#define MAKE_ARG_LIST_75(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_74(op, __VA_ARGS__))
#define MAKE_ARG_LIST_76(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_75(op, __VA_ARGS__))
#define MAKE_ARG_LIST_77(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_76(op, __VA_ARGS__))
#define MAKE_ARG_LIST_78(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_77(op, __VA_ARGS__))
#define MAKE_ARG_LIST_79(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_78(op, __VA_ARGS__))
#define MAKE_ARG_LIST_80(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_79(op, __VA_ARGS__))
#define MAKE_ARG_LIST_81(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_80(op, __VA_ARGS__))
#define MAKE_ARG_LIST_82(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_81(op, __VA_ARGS__))
#define MAKE_ARG_LIST_83(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_82(op, __VA_ARGS__))
#define MAKE_ARG_LIST_84(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_83(op, __VA_ARGS__))
#define MAKE_ARG_LIST_85(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_84(op, __VA_ARGS__))
#define MAKE_ARG_LIST_86(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_85(op, __VA_ARGS__))
#define MAKE_ARG_LIST_87(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_86(op, __VA_ARGS__))
#define MAKE_ARG_LIST_88(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_87(op, __VA_ARGS__))
#define MAKE_ARG_LIST_89(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_88(op, __VA_ARGS__))
#define MAKE_ARG_LIST_90(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_89(op, __VA_ARGS__))
#define MAKE_ARG_LIST_91(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_90(op, __VA_ARGS__))
#define MAKE_ARG_LIST_92(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_91(op, __VA_ARGS__))
#define MAKE_ARG_LIST_93(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_92(op, __VA_ARGS__))
#define MAKE_ARG_LIST_94(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_93(op, __VA_ARGS__))
#define MAKE_ARG_LIST_95(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_94(op, __VA_ARGS__))
#define MAKE_ARG_LIST_96(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_95(op, __VA_ARGS__))
#define MAKE_ARG_LIST_97(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_96(op, __VA_ARGS__))
#define MAKE_ARG_LIST_98(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_97(op, __VA_ARGS__))
#define MAKE_ARG_LIST_99(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_98(op, __VA_ARGS__))
#define MAKE_ARG_LIST_100(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_99(op, __VA_ARGS__))
#define MAKE_ARG_LIST_101(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_100(op, __VA_ARGS__))
#define MAKE_ARG_LIST_102(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_101(op, __VA_ARGS__))
#define MAKE_ARG_LIST_103(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_102(op, __VA_ARGS__))
#define MAKE_ARG_LIST_104(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_103(op, __VA_ARGS__))
#define MAKE_ARG_LIST_105(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_104(op, __VA_ARGS__))
#define MAKE_ARG_LIST_106(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_105(op, __VA_ARGS__))
#define MAKE_ARG_LIST_107(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_106(op, __VA_ARGS__))
#define MAKE_ARG_LIST_108(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_107(op, __VA_ARGS__))
#define MAKE_ARG_LIST_109(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_108(op, __VA_ARGS__))
#define MAKE_ARG_LIST_110(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_109(op, __VA_ARGS__))
#define MAKE_ARG_LIST_111(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_110(op, __VA_ARGS__))
#define MAKE_ARG_LIST_112(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_111(op, __VA_ARGS__))
#define MAKE_ARG_LIST_113(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_112(op, __VA_ARGS__))
#define MAKE_ARG_LIST_114(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_113(op, __VA_ARGS__))
#define MAKE_ARG_LIST_115(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_114(op, __VA_ARGS__))
#define MAKE_ARG_LIST_116(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_115(op, __VA_ARGS__))
#define MAKE_ARG_LIST_117(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_116(op, __VA_ARGS__))
#define MAKE_ARG_LIST_118(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_117(op, __VA_ARGS__))
#define MAKE_ARG_LIST_119(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_118(op, __VA_ARGS__))
#define MAKE_ARG_LIST_120(op, arg, ...)  op(arg), MARCO_EXPAND(MAKE_ARG_LIST_119(op, __VA_ARGS__))

#define RSEQ_N() 						\
	119,118,117,116,115,114,113,112,111,110,		\
	109,108,107,106,105,104,103,102,101,100,		\
	99,98,97,96,95,94,93,92,91,90, 				\
	89,88,87,86,85,84,83,82,81,80, 				\
	79,78,77,76,75,74,73,72,71,70, 				\
	69,68,67,66,65,64,63,62,61,60, 				\
	59,58,57,56,55,54,53,52,51,50, 				\
	49,48,47,46,45,44,43,42,41,40, 				\
	39,38,37,36,35,34,33,32,31,30, 				\
	29,28,27,26,25,24,23,22,21,20, 				\
	19,18,17,16,15,14,13,12,11,10, 				\
	9,8,7,6,5,4,3,2,1,0

#define ARG_N( 							\
	_1, _2, _3, _4, _5, _6, _7, _8, _9,_10, 		\
	_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, 		\
	_21,_22,_23,_24,_25,_26,_27,_28,_29,_30, 		\
	_31,_32,_33,_34,_35,_36,_37,_38,_39,_40, 		\
	_41,_42,_43,_44,_45,_46,_47,_48,_49,_50, 		\
	_51,_52,_53,_54,_55,_56,_57,_58,_59,_60, 		\
	_61,_62,_63,_64,_65,_66,_67,_68,_69,_70, 		\
	_71,_72,_73,_74,_75,_76,_77,_78,_79,_80, 		\
	_81,_82,_83,_84,_85,_86,_87,_88,_89,_90, 		\
	_91,_92,_93,_94,_95,_96,_97,_98,_99,_100, 		\
	_101,_102,_103,_104,_105,_106,_107,_108,_109,_110, 	\
	_111,_112,_113,_114,_115,_116,_117,_118,_119,N, ...) N

#define MARCO_EXPAND(...)                	 __VA_ARGS__
#define APPLY_VARIADIC_MACRO(macro, ...)  	MARCO_EXPAND(macro(__VA_ARGS__))

#define ADD_REFERENCE(t)        		std::reference_wrapper<decltype(t)>(t)
#define ADD_REFERENCE_CONST(t)  		std::reference_wrapper<std::add_const_t<decltype(t)>>(t)
#define OBJECT(t)          			t
#define MAKE_NAMES(...) 			#__VA_ARGS__

#define MACRO_CONCAT(A, B) 			MACRO_CONCAT1(A, B)
#define MACRO_CONCAT1(A, B) 			A##_##B

#define MAKE_ARG_LIST(N, op, arg, ...) 		MACRO_CONCAT(MAKE_ARG_LIST, N)(op, arg, __VA_ARGS__)

#define GET_ARG_COUNT_INNER(...)    		MARCO_EXPAND(ARG_N(__VA_ARGS__))
#define GET_ARG_COUNT(...)          		GET_ARG_COUNT_INNER(__VA_ARGS__, RSEQ_N())

#define BYTE_ARRAY_SERIALIZS_CONCAT(A) serialize.add_from(value.A)
#define BYTE_ARRAY_DESERIALIZS_CONCAT(A) deserialize.get_from(value.A)
#define RAPIDJSON_SERIALIZS_CONCAT(A) _serialize.add_from(#A, cls.A)
#define RAPIDJSON_DESERIALIZS_CONCAT(A) _deserializeValue.get_from(#A, cls.A)
#define TOML11_DESERIALIZS_CONCAT(A) obj.A = toml::find<decltype(obj.A)>(v, #A)
#define TOML11_SERIALIZS_CONCAT(A) v[#A] = obj.A
template<typename...> using void_t = void;

template<typename> struct members_reflection {};

#define MAKE_TUPLE(STRUCT_NAME, N, ...)											\
	template<>struct members_reflection<STRUCT_NAME>{								\
		constexpr decltype(auto) static apply(){								\
			return std::make_tuple(MAKE_ARG_LIST(N, &STRUCT_NAME::OBJECT, __VA_ARGS__));			\
		}													\
		using type = void;											\
		constexpr static char *name = #STRUCT_NAME;								\
		constexpr static size_t value = GET_ARG_COUNT(__VA_ARGS__);						\
		std::array<const char*, GET_ARG_COUNT(__VA_ARGS__)> arr={MAKE_ARG_LIST(N, MAKE_NAMES, __VA_ARGS__)};	\
		static void to_byte_array(crazy::byte_array::Serialize& serialize, const STRUCT_NAME& value) {		\
			MAKE_ARG_LIST(N, BYTE_ARRAY_SERIALIZS_CONCAT, __VA_ARGS__);					\
		}													\
		static void from_byte_array(crazy::byte_array::Deserialize& deserialize, STRUCT_NAME& value) {		\
			MAKE_ARG_LIST(N, BYTE_ARRAY_DESERIALIZS_CONCAT, __VA_ARGS__);					\
		}													\
		static void to_json(crazy::json::Serialize& _serialize ,const STRUCT_NAME& cls) { 			\
			MAKE_ARG_LIST(N, RAPIDJSON_SERIALIZS_CONCAT, __VA_ARGS__);					\
		}													\
		static void from_json(crazy::json::Value _deserializeValue, STRUCT_NAME& cls) {				\
			MAKE_ARG_LIST(N, RAPIDJSON_DESERIALIZS_CONCAT, __VA_ARGS__);					\
		}													\
	};														\
															\
	namespace crazy {												\
		template <>												\
		struct TomlValueCast <STRUCT_NAME, toml::basic_value<toml::preserve_comments>> {			\
			STRUCT_NAME operator()(const toml::basic_value<toml::preserve_comments>& v) {			\
				try {											\
					STRUCT_NAME obj{};								\
					MAKE_ARG_LIST(N, TOML11_DESERIALIZS_CONCAT, __VA_ARGS__);			\
					return obj;									\
				} catch (std::exception& e) {								\
					CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : "				\
						<< e.what();								\
					return STRUCT_NAME {};								\
				}											\
			}												\
		};													\
		template <>												\
		struct TomlValueCast <toml::basic_value<toml::preserve_comments>, STRUCT_NAME> {			\
			toml::basic_value<toml::preserve_comments> operator()(const STRUCT_NAME& obj) {			\
				try {											\
					toml::value v = toml::table{};							\
					MAKE_ARG_LIST(N, TOML11_SERIALIZS_CONCAT, __VA_ARGS__);				\
					return v;									\
				} catch (std::exception& e) {								\
					CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : "				\
						<< e.what();								\
					return {};									\
				}											\
			}												\
		};													\
	}

#define EMMBED_TUPLE(STRUCT_NAME, N, ...) 										\
MAKE_TUPLE(STRUCT_NAME, N, __VA_ARGS__)

#define REFLECTION(STRUCT_NAME, ...) EMMBED_TUPLE(STRUCT_NAME, GET_ARG_COUNT(__VA_ARGS__), __VA_ARGS__)

#endif // ! ____CRAZY_REFLECTION_H____
