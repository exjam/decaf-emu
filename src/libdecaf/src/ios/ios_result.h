#pragma once
#include "ios_enum.h"

namespace ios
{

template<typename ValueType>
class Result
{
public:
   Result(ValueType &&value) :
      mError(Error::OK),
      mValue(std::move(value))
   {
   }

   Result(const ValueType &value) :
      mError(Error::OK),
      mValue(value)
   {
   }

   Result(IOSError error) :
      mError(error)
   {
   }

   Result(IOSError error, const ValueType &value) :
      mError(error),
      mValue(value)
   {
   }

   ValueType value()
   {
      return mValue;
   }

   IOSError error() const
   {
      return mError;
   }

   explicit operator bool() const
   {
      return mError == IOSError::OK;
   }

   operator IOSError() const
   {
      return mError;
   }

   operator ValueType() const
   {
      return mValue;
   }

private:
   IOSError mError;
   ValueType mValue;
};

template<>
class Result<IOSError>
{
public:
   Result(IOSError error) :
      mError(error)
   {
   }

   IOSError error() const
   {
      return mError;
   }

   explicit operator bool() const
   {
      return mError == IOSError::OK;
   }

   operator IOSError() const
   {
      return mError;
   }

private:
   IOSError mError;
};

} // namespace ios
