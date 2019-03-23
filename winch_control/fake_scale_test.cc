#include "scale_filter.h"
#include "gtest/gtest.h"

namespace {

class FakeScaleTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  FakeScaleTest() : scale_("") {
     // You can do set-up work for each test here.
     scale_.DisableForTest();
     scale_.InitLoop(std::bind(&FakeScaleTest::ErrorCallback, this));
     scale_.EnableDrainingAlarm(std::bind(&FakeScaleTest::DrainCallback, this));
     fake_scale_ptr_ = scale_.GetFakeScale();
     fake_scale_ptr_->StopLoop(); // now we can put in data ourselves
  }

  ~FakeScaleTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  void DrainCallback() {
     drain_callbacks_++;
  }

  void ErrorCallback() {
     err_callbacks_++;
  }

  int err_callbacks_ = 0;
  int drain_callbacks_ = 0;

  // Objects declared here can be used by all tests in the test case for Foo.
  ScaleFilter scale_;
  FakeScale *fake_scale_ptr_;
};


// Tests that Foo does Xyz.
TEST_F(FakeScaleTest, StartWithZero) {
  // At first, no readings recorded
  EXPECT_EQ(scale_.GetWeight(), 0.0);
  // Then we add data, and boom!
  fake_scale_ptr_->InputData(1000.0, 100);
  // fake_scale_ptr_->InputData(1000.0, 100);
  // fake_scale_ptr_->InputData(1000.0, 100);
  EXPECT_NE(scale_.GetWeight(), 0.0);
}

TEST_F(FakeScaleTest, DrainCheck) {
   int64_t faketime = 10;
   int64_t interval = 100;
   // no calibration for the filter means that data from the raw data
   // will be treated as grams.
  EXPECT_EQ(drain_callbacks_, 0);
  for (int i=0; i < 50; ++i) {
    fake_scale_ptr_->InputData(1000.0, faketime); faketime+=interval;
    EXPECT_EQ(drain_callbacks_, 0);
  }
  usleep(500000);  // wait interval to trigger a check
  fake_scale_ptr_->InputData(1000.0, faketime); faketime+=interval;
  EXPECT_EQ(drain_callbacks_, 0);

  // Now simulate draining:
  double fakeweight = 1000.0;
  double diff = 15;  // 50 ml per second
  for (int i=0; i < 50; ++i) {
    fake_scale_ptr_->InputData(fakeweight, faketime);
    faketime += interval;
    fakeweight -= diff;
    EXPECT_EQ(drain_callbacks_, 0);
  }
  usleep(600000);  // wait interval to trigger a check
  fake_scale_ptr_->InputData(fakeweight, faketime);
  faketime += interval;
  fakeweight -= diff;
  EXPECT_EQ(drain_callbacks_, 1);

}

TEST_F(FakeScaleTest, EmptyCheck) {
  EXPECT_FALSE(scale_.CheckEmpty());
  fake_scale_ptr_->InputData(12000.0, 100);
  EXPECT_FALSE(scale_.CheckEmpty());
  fake_scale_ptr_->InputData(7000.0, 120);
  EXPECT_TRUE(scale_.CheckEmpty());
}

}  // namespace

// int main(int argc, char **argv) {
  // ::testing::InitGoogleTest(&argc, argv);
  // return RUN_ALL_TESTS();
// }
