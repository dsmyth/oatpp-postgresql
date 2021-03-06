/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#ifndef oatpp_postgresql_mapping_Deserializer_hpp
#define oatpp_postgresql_mapping_Deserializer_hpp

#include "PgArray.hpp"

#include "oatpp/core/data/stream/BufferStream.hpp"
#include "oatpp/core/data/mapping/TypeResolver.hpp"
#include "oatpp/core/Types.hpp"

#include <libpq-fe.h>

#if defined(WIN32) || defined(_WIN32)
  #include <WinSock2.h>
#else
  #include <arpa/inet.h>
#endif

namespace oatpp { namespace postgresql { namespace mapping {

/**
 * Mapper from PostgreSQL values to oatpp values.
 */
class Deserializer {
public:

  struct InData {

    InData() = default;

    InData(PGresult* dbres, int row, int col, const std::shared_ptr<const data::mapping::TypeResolver>& pTypeResolver);

    std::shared_ptr<const data::mapping::TypeResolver> typeResolver;

    Oid oid;
    const char* data;
    v_buff_size size;
    bool isNull;

  };

public:
  typedef oatpp::Void (*DeserializerMethod)(const Deserializer*, const InData&, const Type*);
private:
  static v_int16 deInt2(const InData& data);
  static v_int32 deInt4(const InData& data);
  static v_int64 deInt8(const InData& data);
  static v_int64 deInt(const InData& data);

  static const oatpp::Type* guessAnyType(const InData& data);
private:
  std::vector<DeserializerMethod> m_methods;
public:

  Deserializer();

  void setDeserializerMethod(const data::mapping::type::ClassId& classId, DeserializerMethod method);

  oatpp::Void deserialize(const InData& data, const Type* type) const;

private:

  static oatpp::Void deserializeString(const Deserializer* _this, const InData& data, const Type* type);

  template<class IntWrapper>
  static oatpp::Void deserializeInt(const Deserializer* _this, const InData& data, const Type* type) {
    (void) _this;
    (void) type;
    if(data.isNull) {
      return IntWrapper();
    }
    auto value = deInt(data);
    return IntWrapper((typename IntWrapper::UnderlyingType) value);
  }

  static oatpp::Void deserializeFloat32(const Deserializer* _this, const InData& data, const Type* type);
  static oatpp::Void deserializeFloat64(const Deserializer* _this, const InData& data, const Type* type);

  static oatpp::Void deserializeBoolean(const Deserializer* _this, const InData& data, const Type* type);

  static oatpp::Void deserializeEnum(const Deserializer* _this, const InData& data, const Type* type);

  static oatpp::Void deserializeAny(const Deserializer* _this, const InData& data, const Type* type);

  static oatpp::Void deserializeUuid(const Deserializer* _this, const InData& data, const Type* type);

  template<typename T>
  static const oatpp::Type* generateMultidimensionalArrayType(const InData& data) {

    if(data.size < sizeof(v_int32)) {
      return nullptr;
    }

    auto ndim = (v_int32) ntohl(*((p_int32)data.data));

    switch (ndim) {

      case 0:  return MultidimensionalArray<T, 1>::getClassType();
      case 1:  return MultidimensionalArray<T, 1>::getClassType();
      case 2:  return MultidimensionalArray<T, 2>::getClassType();
      case 3:  return MultidimensionalArray<T, 3>::getClassType();
      case 4:  return MultidimensionalArray<T, 4>::getClassType();
      case 5:  return MultidimensionalArray<T, 5>::getClassType();
      case 6:  return MultidimensionalArray<T, 6>::getClassType();
      case 7:  return MultidimensionalArray<T, 7>::getClassType();
      case 8:  return MultidimensionalArray<T, 8>::getClassType();
      case 9:  return MultidimensionalArray<T, 9>::getClassType();
      case 10: return MultidimensionalArray<T, 10>::getClassType(); // Max 10 dimensions should be enough :)

      default:
        break;

    }

    return nullptr;

  }

  struct ArrayDeserializationMeta {

    ArrayDeserializationMeta(const Deserializer* p_this,
                             const InData* pData)
      : _this(p_this)
      , data(pData)
      , stream(nullptr, (p_char8)pData->data, pData->size)
    {
      ArrayUtils::readArrayHeader(&stream, arrayHeader, dimensions);
    }

    const Deserializer* _this;
    const InData* data;
    data::stream::BufferInputStream stream;
    PgArrayHeader arrayHeader;
    std::vector<v_int32> dimensions;

  };

  static oatpp::Void deserializeSubArray(const Type* type,
                                         ArrayDeserializationMeta& meta,
                                         v_int32 dimension);

  template<class Collection>
  static oatpp::Void deserializeSubArray(const Type* type,
                                         ArrayDeserializationMeta& meta,
                                         v_int32 dimension)
  {

    if(dimension < meta.dimensions.size() - 1) {

      auto polymorphicDispatcher = static_cast<const typename Collection::Class::PolymorphicDispatcher*>(type->polymorphicDispatcher);
      auto itemType = *type->params.begin();
      auto listWrapper = polymorphicDispatcher->createObject();

      auto size = meta.dimensions[dimension];

      for(v_int32 i = 0; i < size; i ++) {
        const auto& item = deserializeSubArray(itemType, meta, dimension + 1);
        polymorphicDispatcher->addPolymorphicItem(listWrapper, item);
      }

      return oatpp::Void(listWrapper.getPtr(), listWrapper.valueType);

    } else if(dimension == meta.dimensions.size() - 1) {

      auto polymorphicDispatcher = static_cast<const typename Collection::Class::PolymorphicDispatcher*>(type->polymorphicDispatcher);
      auto itemType = *type->params.begin();
      auto listWrapper = polymorphicDispatcher->createObject();

      auto size = meta.dimensions[dimension];

      for(v_int32 i = 0; i < size; i ++) {

        v_int32 dataSize;
        meta.stream.readSimple(&dataSize, sizeof(v_int32));

        InData itemData;
        itemData.typeResolver = meta.data->typeResolver;
        itemData.size = (v_int32) ntohl(dataSize);
        itemData.data = (const char*) &meta.stream.getData()[meta.stream.getCurrentPosition()];
        itemData.oid = meta.arrayHeader.oid;
        itemData.isNull = itemData.size < 0;

        if(itemData.size > 0) {
          meta.stream.setCurrentPosition(meta.stream.getCurrentPosition() + itemData.size);
        }

        const auto& item = meta._this->deserialize(itemData, itemType);

        polymorphicDispatcher->addPolymorphicItem(listWrapper, item);

      }

      return oatpp::Void(listWrapper.getPtr(), listWrapper.valueType);

    }

    throw std::runtime_error("[oatpp::postgresql::mapping::Deserializer::deserializeSubArray()]: "
                             "Error. Invalid state: dimension >= dimensions.size().");

  }

  template<class Collection>
  static oatpp::Void deserializeArray(const Deserializer* _this, const InData& data, const Type* type) {

    if(data.isNull) {
      return oatpp::Void(nullptr, type);
    }

    auto ndim = (v_int32) ntohl(*((p_int32)data.data));
    if(ndim == 0) {
      auto polymorphicDispatcher = static_cast<const typename Collection::Class::PolymorphicDispatcher*>(type->polymorphicDispatcher);
      return polymorphicDispatcher->createObject(); // empty array
    }

    ArrayDeserializationMeta meta(_this, &data);
    return deserializeSubArray(type, meta, 0);

  }

};

}}}

#endif // oatpp_postgresql_mapping_Deserializer_hpp
