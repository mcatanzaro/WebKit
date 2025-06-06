/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "DateConstructor.h"

#include "DateConversion.h"
#include "DateInstance.h"
#include "DatePrototype.h"
#include "JSCInlines.h"
#include "JSDateMath.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(dateParse);
static JSC_DECLARE_HOST_FUNCTION(dateUTC);
static JSC_DECLARE_HOST_FUNCTION(dateNow);

}

#include "DateConstructor.lut.h"

namespace JSC {

const ClassInfo DateConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &dateConstructorTable, nullptr, CREATE_METHOD_TABLE(DateConstructor) };

/* Source for DateConstructor.lut.h
@begin dateConstructorTable
  parse     dateParse   DontEnum|Function 1
  UTC       dateUTC     DontEnum|Function 7
  now       dateNow     DontEnum|Function 0
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DateConstructor);

static JSC_DECLARE_HOST_FUNCTION(callDate);
static JSC_DECLARE_HOST_FUNCTION(constructWithDateConstructor);

DateConstructor::DateConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callDate, constructWithDateConstructor)
{
}

void DateConstructor::finishCreation(VM& vm, DatePrototype* datePrototype)
{
    Base::finishCreation(vm, 7, vm.propertyNames->Date.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, datePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static inline double toIntegerOrInfinity(double d)
{
    return trunc(std::isnan(d) ? 0.0 : d + 0.0);
}

// https://tc39.es/ecma262/#sec-makedate
static inline double makeDate(double day, double time)
{
#if COMPILER(CLANG)
    #pragma STDC FP_CONTRACT OFF
#endif
    return (day * msPerDay) + time;
}

// https://tc39.es/ecma262/#sec-maketime
static inline double makeTime(double hour, double min, double sec, double ms)
{
#if COMPILER(CLANG)
    #pragma STDC FP_CONTRACT OFF
#endif
    return (((hour * msPerHour) + min * msPerMinute) + sec * msPerSecond) + ms;
}

// https://tc39.es/ecma262/#sec-makeday
static inline double makeDay(double year, double month, double date)
{
    double additionalYears = std::floor(month / 12);
    double ym = year + additionalYears;
    if (!std::isfinite(ym))
        return PNaN;
    double mm = month - additionalYears * 12;
    int32_t yearInt32 = toInt32(ym);
    int32_t monthInt32 = toInt32(mm);
    if (yearInt32 != ym || monthInt32 != mm)
        return PNaN;
    double days = dateToDaysFrom1970(yearInt32, monthInt32, 1);
    return days + date - 1;
}

static double millisecondsFromComponents(JSGlobalObject* globalObject, const ArgList& args, TimeType timeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Initialize doubleArguments with default values.
    double doubleArguments[7] {
        0, 0, 1, 0, 0, 0, 0
    };
    unsigned numberOfUsedArguments = std::max(std::min<unsigned>(7U, args.size()), 1U);
    for (unsigned i = 0; i < numberOfUsedArguments; ++i) {
        doubleArguments[i] = args.at(i).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);
    }
    for (unsigned i = 0; i < numberOfUsedArguments; ++i) {
        if (!std::isfinite(doubleArguments[i]))
            return PNaN;
        doubleArguments[i] = toIntegerOrInfinity(doubleArguments[i]);
    }

    if (0 <= doubleArguments[0] && doubleArguments[0] <= 99)
        doubleArguments[0] += 1900;

    double time = makeDate(makeDay(doubleArguments[0], doubleArguments[1], doubleArguments[2]), makeTime(doubleArguments[3], doubleArguments[4], doubleArguments[5], doubleArguments[6]));
    if (!std::isfinite(time))
        return PNaN;
    return timeClip(vm.dateCache.localTimeToMS(time, timeType));
}

// ECMA 15.9.3
JSObject* constructDate(JSGlobalObject* globalObject, JSValue newTarget, const ArgList& args)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    int numArgs = args.size();

    double value;

    if (numArgs == 0) // new Date() ECMA 15.9.3.3
        value = jsCurrentTime();
    else if (numArgs == 1) {
        JSValue arg0 = args.at(0);
        if (auto* dateInstance = jsDynamicCast<DateInstance*>(arg0))
            value = dateInstance->internalNumber();
        else {
            JSValue primitive = arg0.toPrimitive(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (primitive.isString()) {
                auto primitiveString = asString(primitive)->value(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
                value = vm.dateCache.parseDate(globalObject, vm, primitiveString);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else {
                value = primitive.toNumber(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
            }
        }
    } else {
        value = millisecondsFromComponents(globalObject, args, TimeType::LocalTime);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    Structure* dateStructure = nullptr;
    if (!newTarget)
        dateStructure = globalObject->dateStructure();
    else {
        dateStructure = JSC_GET_DERIVED_STRUCTURE(vm, dateStructure, asObject(newTarget), globalObject->dateConstructor());
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    return DateInstance::create(vm, dateStructure, value);
}
    
JSC_DEFINE_HOST_FUNCTION(constructWithDateConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructDate(globalObject, callFrame->newTarget(), args));
}

// ECMA 15.9.2
JSC_DEFINE_HOST_FUNCTION(callDate, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    GregorianDateTime ts;
    vm.dateCache.msToGregorianDateTime(WallTime::now().secondsSinceEpoch().milliseconds(), TimeType::LocalTime, ts);
    return JSValue::encode(jsNontrivialString(vm, formatDateTime(ts, DateTimeFormatDateAndTime, false, vm.dateCache)));
}

JSC_DEFINE_HOST_FUNCTION(dateParse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String dateStr = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(timeClip(vm.dateCache.parseDate(globalObject, vm, dateStr)))));
}

JSValue dateNowImpl()
{
    return jsNumber(jsCurrentTime());
}

JSC_DEFINE_HOST_FUNCTION(dateNow, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(jsCurrentTime()));
}

JSC_DEFINE_HOST_FUNCTION(dateUTC, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    double ms = millisecondsFromComponents(globalObject, ArgList(callFrame), TimeType::UTCTime);
    return JSValue::encode(jsNumber(ms));
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
