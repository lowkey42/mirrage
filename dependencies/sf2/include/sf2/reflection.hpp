/***********************************************************\
 * Macros to auto-ish generate struct/enum descriptions    *
 *     ___________ _____                                   *
 *    /  ___|  ___/ __  \                                  *
 *    \ `--.| |_  `' / /'                                  *
 *     `--. \  _|   / /                                    *
 *    /\__/ / |   ./ /___                                  *
 *    \____/\_|   \_____/                                  *
 *                                                         *
 *                                                         *
 *  Copyright (c) 2014 Florian Oetke                       *
 *                                                         *
 *  This file is part of SF2 and distributed under         *
 *  the MIT License. See LICENSE file for details.         *
\***********************************************************/

#pragma once

#include "reflection_data.hpp"

namespace sf2 {}

// clang-format off

/* This counts the number of args */
#define SF2_NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_64,_65,_66,_67,_68,_69,_70,_71,_72,_73,_74,_75,_76,_77,_78,_79,_80,_81,_82,_83,_84,_85,_86,_87,_88,_89,_90,_91,_92,_93,_94,_95,_96,_97,_98,_99,_100,_101,_102,_103,_104,_105,_106,_107,_108,_109,_110,_111,_112,_113,_114,_115,_116,_117,_118,_119,_120,_121,_122,_123,_124,_125,_126,N,...) N

#define SF2_NARGS(...) SF2_NARGS_SEQ(__VA_ARGS__, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/* This will let macros expand before concating them */
#define SF2_PRIMITIVE_CAT(x, y) x ## y
#define SF2_CAT(x, y) SF2_PRIMITIVE_CAT(x, y)

/* This will call a macro on each argument passed in */
#define SF2_APPLY(macro, ...) SF2_CAT(SF2_APPLY_, SF2_NARGS(__VA_ARGS__))(macro, __VA_ARGS__)
#define SF2_APPLY_1(m, x1) m(x1)
#define SF2_APPLY_2(m, x, ...) m(x), SF2_APPLY_1(m,__VA_ARGS__)
#define SF2_APPLY_3(m, x, ...) m(x), SF2_APPLY_2(m,__VA_ARGS__)
#define SF2_APPLY_4(m, x, ...) m(x), SF2_APPLY_3(m,__VA_ARGS__)
#define SF2_APPLY_5(m, x, ...) m(x), SF2_APPLY_4(m,__VA_ARGS__)
#define SF2_APPLY_6(m, x, ...) m(x), SF2_APPLY_5(m,__VA_ARGS__)
#define SF2_APPLY_7(m, x, ...) m(x), SF2_APPLY_6(m,__VA_ARGS__)
#define SF2_APPLY_8(m, x, ...) m(x), SF2_APPLY_7(m,__VA_ARGS__)
#define SF2_APPLY_9(m, x, ...) m(x), SF2_APPLY_8(m,__VA_ARGS__)
#define SF2_APPLY_10(m, x, ...) m(x), SF2_APPLY_9(m,__VA_ARGS__)
#define SF2_APPLY_11(m, x, ...) m(x), SF2_APPLY_10(m,__VA_ARGS__)
#define SF2_APPLY_12(m, x, ...) m(x), SF2_APPLY_11(m,__VA_ARGS__)
#define SF2_APPLY_13(m, x, ...) m(x), SF2_APPLY_12(m,__VA_ARGS__)
#define SF2_APPLY_14(m, x, ...) m(x), SF2_APPLY_13(m,__VA_ARGS__)
#define SF2_APPLY_15(m, x, ...) m(x), SF2_APPLY_14(m,__VA_ARGS__)
#define SF2_APPLY_16(m, x, ...) m(x), SF2_APPLY_15(m,__VA_ARGS__)
#define SF2_APPLY_17(m, x, ...) m(x), SF2_APPLY_16(m,__VA_ARGS__)
#define SF2_APPLY_18(m, x, ...) m(x), SF2_APPLY_17(m,__VA_ARGS__)
#define SF2_APPLY_19(m, x, ...) m(x), SF2_APPLY_18(m,__VA_ARGS__)
#define SF2_APPLY_20(m, x, ...) m(x), SF2_APPLY_19(m,__VA_ARGS__)
#define SF2_APPLY_21(m, x, ...) m(x), SF2_APPLY_20(m,__VA_ARGS__)
#define SF2_APPLY_22(m, x, ...) m(x), SF2_APPLY_21(m,__VA_ARGS__)
#define SF2_APPLY_23(m, x, ...) m(x), SF2_APPLY_22(m,__VA_ARGS__)
#define SF2_APPLY_24(m, x, ...) m(x), SF2_APPLY_23(m,__VA_ARGS__)
#define SF2_APPLY_25(m, x, ...) m(x), SF2_APPLY_24(m,__VA_ARGS__)
#define SF2_APPLY_26(m, x, ...) m(x), SF2_APPLY_25(m,__VA_ARGS__)
#define SF2_APPLY_27(m, x, ...) m(x), SF2_APPLY_26(m,__VA_ARGS__)
#define SF2_APPLY_28(m, x, ...) m(x), SF2_APPLY_27(m,__VA_ARGS__)
#define SF2_APPLY_29(m, x, ...) m(x), SF2_APPLY_28(m,__VA_ARGS__)
#define SF2_APPLY_30(m, x, ...) m(x), SF2_APPLY_29(m,__VA_ARGS__)
#define SF2_APPLY_31(m, x, ...) m(x), SF2_APPLY_30(m,__VA_ARGS__)
#define SF2_APPLY_32(m, x, ...) m(x), SF2_APPLY_31(m,__VA_ARGS__)
#define SF2_APPLY_33(m, x, ...) m(x), SF2_APPLY_32(m,__VA_ARGS__)
#define SF2_APPLY_34(m, x, ...) m(x), SF2_APPLY_33(m,__VA_ARGS__)
#define SF2_APPLY_35(m, x, ...) m(x), SF2_APPLY_34(m,__VA_ARGS__)
#define SF2_APPLY_36(m, x, ...) m(x), SF2_APPLY_35(m,__VA_ARGS__)
#define SF2_APPLY_37(m, x, ...) m(x), SF2_APPLY_36(m,__VA_ARGS__)
#define SF2_APPLY_38(m, x, ...) m(x), SF2_APPLY_37(m,__VA_ARGS__)
#define SF2_APPLY_39(m, x, ...) m(x), SF2_APPLY_38(m,__VA_ARGS__)
#define SF2_APPLY_40(m, x, ...) m(x), SF2_APPLY_39(m,__VA_ARGS__)
#define SF2_APPLY_41(m, x, ...) m(x), SF2_APPLY_40(m,__VA_ARGS__)
#define SF2_APPLY_42(m, x, ...) m(x), SF2_APPLY_41(m,__VA_ARGS__)
#define SF2_APPLY_43(m, x, ...) m(x), SF2_APPLY_42(m,__VA_ARGS__)
#define SF2_APPLY_44(m, x, ...) m(x), SF2_APPLY_43(m,__VA_ARGS__)
#define SF2_APPLY_45(m, x, ...) m(x), SF2_APPLY_44(m,__VA_ARGS__)
#define SF2_APPLY_46(m, x, ...) m(x), SF2_APPLY_45(m,__VA_ARGS__)
#define SF2_APPLY_47(m, x, ...) m(x), SF2_APPLY_46(m,__VA_ARGS__)
#define SF2_APPLY_48(m, x, ...) m(x), SF2_APPLY_47(m,__VA_ARGS__)
#define SF2_APPLY_49(m, x, ...) m(x), SF2_APPLY_48(m,__VA_ARGS__)
#define SF2_APPLY_50(m, x, ...) m(x), SF2_APPLY_49(m,__VA_ARGS__)
#define SF2_APPLY_51(m, x, ...) m(x), SF2_APPLY_50(m,__VA_ARGS__)
#define SF2_APPLY_52(m, x, ...) m(x), SF2_APPLY_51(m,__VA_ARGS__)
#define SF2_APPLY_53(m, x, ...) m(x), SF2_APPLY_52(m,__VA_ARGS__)
#define SF2_APPLY_54(m, x, ...) m(x), SF2_APPLY_53(m,__VA_ARGS__)
#define SF2_APPLY_55(m, x, ...) m(x), SF2_APPLY_54(m,__VA_ARGS__)
#define SF2_APPLY_56(m, x, ...) m(x), SF2_APPLY_55(m,__VA_ARGS__)
#define SF2_APPLY_57(m, x, ...) m(x), SF2_APPLY_56(m,__VA_ARGS__)
#define SF2_APPLY_58(m, x, ...) m(x), SF2_APPLY_57(m,__VA_ARGS__)
#define SF2_APPLY_59(m, x, ...) m(x), SF2_APPLY_58(m,__VA_ARGS__)
#define SF2_APPLY_60(m, x, ...) m(x), SF2_APPLY_59(m,__VA_ARGS__)
#define SF2_APPLY_61(m, x, ...) m(x), SF2_APPLY_60(m,__VA_ARGS__)
#define SF2_APPLY_62(m, x, ...) m(x), SF2_APPLY_61(m,__VA_ARGS__)
#define SF2_APPLY_63(m, x, ...) m(x), SF2_APPLY_62(m,__VA_ARGS__)
#define SF2_APPLY_64(m, x, ...) m(x), SF2_APPLY_63(m,__VA_ARGS__)
#define SF2_APPLY_65(m, x, ...) m(x), SF2_APPLY_64(m,__VA_ARGS__)
#define SF2_APPLY_66(m, x, ...) m(x), SF2_APPLY_65(m,__VA_ARGS__)
#define SF2_APPLY_67(m, x, ...) m(x), SF2_APPLY_66(m,__VA_ARGS__)
#define SF2_APPLY_68(m, x, ...) m(x), SF2_APPLY_67(m,__VA_ARGS__)
#define SF2_APPLY_69(m, x, ...) m(x), SF2_APPLY_68(m,__VA_ARGS__)
#define SF2_APPLY_70(m, x, ...) m(x), SF2_APPLY_69(m,__VA_ARGS__)
#define SF2_APPLY_71(m, x, ...) m(x), SF2_APPLY_70(m,__VA_ARGS__)
#define SF2_APPLY_72(m, x, ...) m(x), SF2_APPLY_71(m,__VA_ARGS__)
#define SF2_APPLY_73(m, x, ...) m(x), SF2_APPLY_72(m,__VA_ARGS__)
#define SF2_APPLY_74(m, x, ...) m(x), SF2_APPLY_73(m,__VA_ARGS__)
#define SF2_APPLY_75(m, x, ...) m(x), SF2_APPLY_74(m,__VA_ARGS__)
#define SF2_APPLY_76(m, x, ...) m(x), SF2_APPLY_75(m,__VA_ARGS__)
#define SF2_APPLY_77(m, x, ...) m(x), SF2_APPLY_76(m,__VA_ARGS__)
#define SF2_APPLY_78(m, x, ...) m(x), SF2_APPLY_77(m,__VA_ARGS__)
#define SF2_APPLY_79(m, x, ...) m(x), SF2_APPLY_78(m,__VA_ARGS__)
#define SF2_APPLY_80(m, x, ...) m(x), SF2_APPLY_79(m,__VA_ARGS__)
#define SF2_APPLY_81(m, x, ...) m(x), SF2_APPLY_80(m,__VA_ARGS__)
#define SF2_APPLY_82(m, x, ...) m(x), SF2_APPLY_81(m,__VA_ARGS__)
#define SF2_APPLY_83(m, x, ...) m(x), SF2_APPLY_82(m,__VA_ARGS__)
#define SF2_APPLY_84(m, x, ...) m(x), SF2_APPLY_83(m,__VA_ARGS__)
#define SF2_APPLY_85(m, x, ...) m(x), SF2_APPLY_84(m,__VA_ARGS__)
#define SF2_APPLY_86(m, x, ...) m(x), SF2_APPLY_85(m,__VA_ARGS__)
#define SF2_APPLY_87(m, x, ...) m(x), SF2_APPLY_86(m,__VA_ARGS__)
#define SF2_APPLY_88(m, x, ...) m(x), SF2_APPLY_87(m,__VA_ARGS__)
#define SF2_APPLY_89(m, x, ...) m(x), SF2_APPLY_88(m,__VA_ARGS__)
#define SF2_APPLY_90(m, x, ...) m(x), SF2_APPLY_89(m,__VA_ARGS__)
#define SF2_APPLY_91(m, x, ...) m(x), SF2_APPLY_90(m,__VA_ARGS__)
#define SF2_APPLY_92(m, x, ...) m(x), SF2_APPLY_91(m,__VA_ARGS__)
#define SF2_APPLY_93(m, x, ...) m(x), SF2_APPLY_92(m,__VA_ARGS__)
#define SF2_APPLY_94(m, x, ...) m(x), SF2_APPLY_93(m,__VA_ARGS__)
#define SF2_APPLY_95(m, x, ...) m(x), SF2_APPLY_94(m,__VA_ARGS__)
#define SF2_APPLY_96(m, x, ...) m(x), SF2_APPLY_95(m,__VA_ARGS__)
#define SF2_APPLY_97(m, x, ...) m(x), SF2_APPLY_96(m,__VA_ARGS__)
#define SF2_APPLY_98(m, x, ...) m(x), SF2_APPLY_97(m,__VA_ARGS__)
#define SF2_APPLY_99(m, x, ...) m(x), SF2_APPLY_98(m,__VA_ARGS__)
#define SF2_APPLY_100(m, x, ...) m(x), SF2_APPLY_99(m,__VA_ARGS__)
#define SF2_APPLY_101(m, x, ...) m(x), SF2_APPLY_100(m,__VA_ARGS__)
#define SF2_APPLY_102(m, x, ...) m(x), SF2_APPLY_101(m,__VA_ARGS__)
#define SF2_APPLY_103(m, x, ...) m(x), SF2_APPLY_102(m,__VA_ARGS__)
#define SF2_APPLY_104(m, x, ...) m(x), SF2_APPLY_103(m,__VA_ARGS__)
#define SF2_APPLY_105(m, x, ...) m(x), SF2_APPLY_104(m,__VA_ARGS__)
#define SF2_APPLY_106(m, x, ...) m(x), SF2_APPLY_105(m,__VA_ARGS__)
#define SF2_APPLY_107(m, x, ...) m(x), SF2_APPLY_106(m,__VA_ARGS__)
#define SF2_APPLY_108(m, x, ...) m(x), SF2_APPLY_107(m,__VA_ARGS__)
#define SF2_APPLY_109(m, x, ...) m(x), SF2_APPLY_108(m,__VA_ARGS__)
#define SF2_APPLY_110(m, x, ...) m(x), SF2_APPLY_109(m,__VA_ARGS__)
#define SF2_APPLY_111(m, x, ...) m(x), SF2_APPLY_110(m,__VA_ARGS__)
#define SF2_APPLY_112(m, x, ...) m(x), SF2_APPLY_111(m,__VA_ARGS__)
#define SF2_APPLY_113(m, x, ...) m(x), SF2_APPLY_112(m,__VA_ARGS__)
#define SF2_APPLY_114(m, x, ...) m(x), SF2_APPLY_113(m,__VA_ARGS__)
#define SF2_APPLY_115(m, x, ...) m(x), SF2_APPLY_114(m,__VA_ARGS__)
#define SF2_APPLY_116(m, x, ...) m(x), SF2_APPLY_115(m,__VA_ARGS__)
#define SF2_APPLY_117(m, x, ...) m(x), SF2_APPLY_116(m,__VA_ARGS__)
#define SF2_APPLY_118(m, x, ...) m(x), SF2_APPLY_117(m,__VA_ARGS__)
#define SF2_APPLY_119(m, x, ...) m(x), SF2_APPLY_118(m,__VA_ARGS__)
#define SF2_APPLY_120(m, x, ...) m(x), SF2_APPLY_119(m,__VA_ARGS__)
#define SF2_APPLY_121(m, x, ...) m(x), SF2_APPLY_120(m,__VA_ARGS__)
#define SF2_APPLY_122(m, x, ...) m(x), SF2_APPLY_121(m,__VA_ARGS__)
#define SF2_APPLY_123(m, x, ...) m(x), SF2_APPLY_122(m,__VA_ARGS__)
#define SF2_APPLY_124(m, x, ...) m(x), SF2_APPLY_123(m,__VA_ARGS__)
#define SF2_APPLY_125(m, x, ...) m(x), SF2_APPLY_124(m,__VA_ARGS__)
#define SF2_APPLY_126(m, x, ...) m(x), SF2_APPLY_125(m,__VA_ARGS__)


#define SF2_EXTRACT_VALUE(n) ::std::make_pair(sf2_current_type::n, ::sf2::String_literal{#n})

#define sf2_enumDef(TYPE, ...) inline auto& sf2_enum_info_factory(TYPE*) {\
	using sf2_current_type = TYPE;\
	static auto data = ::sf2::Enum_info<TYPE>{\
			::sf2::String_literal{#TYPE}, \
			{SF2_APPLY(SF2_EXTRACT_VALUE,__VA_ARGS__)}\
	};\
	return data;\
} auto& sf2_enum_info_factory(TYPE*)

#define sf2_enum(NAME, ...) enum NAME {__VA_ARGS__}; sf2_enumDef(NAME, __VA_ARGS__)

#define SF2_EXTRACT_TYPE(n) decltype(sf2_current_type::n)
#define SF2_EXTRACT_MEMBER(n) ::std::make_pair(&sf2_current_type::n, ::sf2::String_literal{#n})

#define sf2_structDef(TYPE, ...) inline auto& sf2_struct_info_factory(TYPE*) {\
	using sf2_current_type = TYPE;\
	static constexpr auto data = ::sf2::Struct_info<TYPE,\
		SF2_APPLY(SF2_EXTRACT_TYPE,__VA_ARGS__)>{\
			::sf2::String_literal{#TYPE}, \
			SF2_APPLY(SF2_EXTRACT_MEMBER,__VA_ARGS__)\
	};\
	return data;\
} auto& sf2_struct_info_factory(TYPE*)

#define sf2_accesor(TYPE) auto& sf2_struct_info_factory(TYPE*)

// clang-format on
