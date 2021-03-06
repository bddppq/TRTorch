#include <string>
#include "gtest/gtest.h"
#include "torch/csrc/jit/irparser.h"
#include "tests/util/util.h"
#include "core/compiler.h"


TEST(Converters, ATenLinearNoBiasConvertsCorrectly) {
    const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(2, 2)):
        %2 : None = prim::Constant()
        %3 : Tensor = aten::linear(%0, %1, %2)
        return (%3))IR";

    auto g = std::make_shared<torch::jit::Graph>();
    torch::jit::script::parseIR(graph, &*g);

    //Input Tensor needs to be 4D for TensorRT linear
    auto in = at::randint(1, 10, {1, 2}, {at::kCUDA});
    auto w = at::randint(1, 10, {3, 2}, {at::kCUDA});
    
    auto params = trtorch::core::conversion::get_named_params(g->inputs(), {w});
    auto jit_results = trtorch::tests::util::RunGraph(g, params, {in});

    in = at::clone(in);
    w = at::clone(w);
    params = trtorch::core::conversion::get_named_params(g->inputs(), {w});
    auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {in});

    ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt_results[0].reshape_as(jit_results[0])));
}


//TODO: Track down the cause of why the JIT linear function fails
//TODO: Sort out what the exepected output dim should be? 
TEST(Converters, ATenLinearBiasConvertsCorrectly) {
    const auto graph = R"IR(
      graph(%0 : Tensor,
            %1 : Float(2, 3),
            %2 : Float(2)):
        %3 : Tensor = aten::linear(%0, %1, %2)
        return (%3))IR";

    auto g = std::make_shared<torch::jit::Graph>();
    torch::jit::script::parseIR(graph, &*g);

    // WARN: TRT expects a 4D input eventually, but pytorch does not require a channel dim
    auto in = at::randint(1, 5, {1, 3}, {at::kCUDA});
    auto w = at::randint(1, 5, {2, 3}, {at::kCUDA});
    auto b = at::randint(1, 5, {2}, {at::kCUDA});

    auto jit_in = at::clone(in);
    auto jit_w = at::clone(w);
    auto jit_b = at::clone(b);
    
    auto params = trtorch::core::conversion::get_named_params(g->inputs(), {jit_w, jit_b});
    auto jit_results = trtorch::tests::util::RunGraph(g, params, {jit_in});

    auto trt_in = at::clone(in);
    auto trt_w = at::clone(w);
    auto trt_b = at::clone(b);
    params = trtorch::core::conversion::get_named_params(g->inputs(), {trt_w, trt_b});
    auto trt_results = trtorch::tests::util::RunGraphEngine(g, params, {trt_in});


    ASSERT_TRUE(trtorch::tests::util::almostEqual(jit_results[0], trt_results[0].reshape_as(jit_results[0])));
}
