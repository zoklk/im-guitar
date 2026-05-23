// Phase 1 — Product 9종 + ProductIdGen 인스턴스 격리 + 단조 증가 검증.

#include "common/Types.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

#include <gtest/gtest.h>

TEST(Product, NineTypesInstantiateWithCorrectType) {
    RawWood        w(1);
    HeadPart       h(2);
    NeckPart       n(3);
    BodyPart       b(4);
    Bridge         br(5);
    Pickup         p(6);
    ElecPartSet    e(7);
    AssembledBody  a(8);
    FinishedGuitar f(9);

    EXPECT_EQ(w.getType(), ProductType::RawWood);
    EXPECT_EQ(h.getType(), ProductType::HeadPart);
    EXPECT_EQ(n.getType(), ProductType::NeckPart);
    EXPECT_EQ(b.getType(), ProductType::BodyPart);
    EXPECT_EQ(br.getType(), ProductType::Bridge);
    EXPECT_EQ(p.getType(), ProductType::Pickup);
    EXPECT_EQ(e.getType(), ProductType::ElecPartSet);
    EXPECT_EQ(a.getType(), ProductType::AssembledBody);
    EXPECT_EQ(f.getType(), ProductType::FinishedGuitar);

    EXPECT_EQ(w.getId(), 1);
    EXPECT_EQ(f.getId(), 9);
}

TEST(Product, BodyPartIsPaintedTogglesViaSetter) {
    BodyPart b(1);
    EXPECT_FALSE(b.isPainted());
    b.setPainted(true);
    EXPECT_TRUE(b.isPainted());
    b.setPainted(false);
    EXPECT_FALSE(b.isPainted());
}

TEST(Product, BaseGetterReturnsConstructedValues) {
    HeadPart h(42);
    EXPECT_EQ(h.getId(), 42);
    EXPECT_EQ(h.getType(), ProductType::HeadPart);
}

TEST(ProductIdGen, NextProducesMonotonicallyIncreasingIds) {
    ProductIdGen gen;
    EXPECT_EQ(gen.next(), 1);
    EXPECT_EQ(gen.next(), 2);
    EXPECT_EQ(gen.next(), 3);
}

TEST(ProductIdGen, SeparateInstancesHaveIndependentCounters) {
    ProductIdGen a;
    ProductIdGen b;

    EXPECT_EQ(a.next(), 1);
    EXPECT_EQ(a.next(), 2);
    EXPECT_EQ(b.next(), 1);
    EXPECT_EQ(a.next(), 3);
    EXPECT_EQ(b.next(), 2);
}

TEST(ProductIdGen, PeekDoesNotAdvance) {
    ProductIdGen gen;
    gen.next();
    gen.next();
    EXPECT_EQ(gen.peek(), 2);
    EXPECT_EQ(gen.peek(), 2);
    EXPECT_EQ(gen.next(), 3);
}

TEST(ProductIdGen, SetCounterRestoresState) {
    ProductIdGen gen;
    gen.setCounter(100);
    EXPECT_EQ(gen.peek(), 100);
    EXPECT_EQ(gen.next(), 101);
}
