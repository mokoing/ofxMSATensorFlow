#include "ofxMSATFUtils.h"

namespace msa {
namespace tf {

#define OFXMSATF_LOG_RETURN_ERROR(expr, msg)    TF_RETURN_IF_ERROR(logError(expr, msg))

//--------------------------------------------------------------
tensorflow::Status log_error(const tensorflow::Status& status, const string msg) {
    if(!status.ok()) {
        string s = msg + " | " + status.ToString();
        ofLogError() << s;
        throw std::runtime_error(s);
    }
    return status;
}


//--------------------------------------------------------------
GraphDef_ptr load_graph_def(const string path, tensorflow::Env* env) {
    string of_path(ofToDataPath(path));
    GraphDef_ptr graph_def(new tensorflow::GraphDef());
    tensorflow::Status status = tensorflow::ReadBinaryProto(env, of_path, graph_def.get());
    if(status.ok()) return graph_def;

    // on error return nullptr
    log_error(status, "Error loading graph " + of_path );
    return nullptr;
}


//--------------------------------------------------------------
Session_ptr create_session_with_graph(
        tensorflow::GraphDef& graph_def,
        const string device,
        const tensorflow::SessionOptions& session_options)
{
    Session_ptr session(NewSession(session_options));
    if( !session) { ofLogError() << "Error creating session"; return nullptr; }

    if(!device.empty())
        tensorflow::graph::SetDefaultDevice(device, &graph_def);

    log_error(session->Create(graph_def), "Error creating graph for session");
    return session;
}


//--------------------------------------------------------------
Session_ptr create_session_with_graph(
        GraphDef_ptr pgraph_def,
        const string device,
        const tensorflow::SessionOptions& session_options)
{
    return create_session_with_graph(*pgraph_def, device, session_options);
}


//--------------------------------------------------------------
Session_ptr create_session_with_graph(
        const string graph_def_path,
        const string device,
        const tensorflow::SessionOptions& session_options)
{
    auto graph_def = load_graph_def(graph_def_path);
    if(graph_def) return create_session_with_graph(graph_def, device, session_options);

    // on error return nullptr
    return nullptr;
}

//--------------------------------------------------------------
vector<tensorflow::int64> tensor_to_pixel_dims(const tensorflow::Tensor &t, string chmap) {
    int rank = t.shape().dims();
    vector<tensorflow::int64> tensor_dims(rank);
    for(int i=0; i<rank; i++) tensor_dims[i] = t.dim_size(i); // useful for debugging

    // add z to end of string to top it up to length 3, this'll make it default to 1
    while(chmap.length()<3) chmap += "z";

    // which tensor dimension to use for which image xyz component
    // initially read from chmap parameter
    ofVec3f dim_indices(chmap[0]-'0', chmap[1]-'0', chmap[2]-'0');

    // if tensor rank is less than the chmap, adjust dim_indices accordingly (
    if(rank < chmap.length()) {
        if(rank == 1) {
            //  if(dim_indices)
            dim_indices.set(0, 99, 99);   // set these large so they default to 1
        } else if(rank == 2) {
            if(dim_indices[1] > dim_indices[0]) dim_indices.set(0, 1, 99);
            else dim_indices.set(1, 0, 99);
        }
    }

    vector<tensorflow::int64> image_dims( {
                (rank > dim_indices[0] ? (int)t.dim_size( dim_indices[0]) : 1),
        (rank > dim_indices[1] ? (int)t.dim_size( dim_indices[1]) : 1),
        (rank > dim_indices[2] ? (int)t.dim_size( dim_indices[2]) : 1)
      });
    return image_dims;
}


//--------------------------------------------------------------
vector<tensorflow::int64> get_imagedims_for_tensorshape(const vector<tensorflow::int64>& tensorshape, bool shape_includes_batch) {
    tensorflow::int64 h_index = shape_includes_batch ? 1 : 0;   // index in shape for image height
    tensorflow::int64 w_index = shape_includes_batch ? 2 : 1;   // index in shape for image width
    tensorflow::int64 c_index = shape_includes_batch ? 3 : 2;   // index in shape for number of channels

    tensorflow::int64 h = h_index < tensorshape.size() ? tensorshape[h_index] : 1; // value for image height
    tensorflow::int64 w = w_index < tensorshape.size() ? tensorshape[w_index] : 1; // value for image width
    tensorflow::int64 c = c_index < tensorshape.size() ? tensorshape[c_index] : 1; // value for image height
    return {w, h, c};
}

//--------------------------------------------------------------
//void get_top_scores(tensorflow::Tensor scores_tensor, int topk_count, vector<int> &out_indices, vector<float> &out_scores, string output_name) {
    //    tensorflow::GraphDefBuilder b;
    //    tensorflow::ops::TopKV2(tensorflow::ops::Const(scores_tensor, b.opts()), tensorflow::ops::Const(topk_count, b.opts()), b.opts().WithName(output_name));

    //    // This runs the GraphDef network definition that we've just constructed, and
    //    // returns the results in the output tensors.
    //    tensorflow::GraphDef graph;
    //    b.ToGraphDef(&graph);

    //    std::unique_ptr<tensorflow::Session> session(tensorflow::NewSession(tensorflow::SessionOptions()));
    //    session->Create(graph);

    //    // The TopK node returns two outputs, the scores and their original indices,
    //    // so we have to append :0 and :1 to specify them both.
    //    std::vector<tensorflow::Tensor> output_tensors;
    //    session->Run({}, {output_name + ":0", output_name + ":1"},{}, &output_tensors);
    //    tensor_to_vector(output_tensors[0], out_scores);
    //    tensor_to_vector(output_tensors[1], out_indices);
//}

void get_topk(const vector<float> probs, vector<int> &out_indices, vector<float> &out_values, int k) {
    // http://stackoverflow.com/questions/14902876/indices-of-the-k-largest-elements-in-an-unsorted-length-n-array
    out_indices.resize(k);
    out_values.resize(k);
    std::priority_queue<std::pair<float, int>> q;
    for (int i = 0; i < probs.size(); ++i) {
        q.push(std::pair<float, int>(probs[i], i));
    }
    for (int i = 0; i < k; ++i) {
        int ki = q.top().second;
        out_indices[i] = ki;
        out_values[i] = probs[ki];
        q.pop();
    }
}

//--------------------------------------------------------------
bool read_labels_file(string file_name, vector<string>& result) {
    std::ifstream file(file_name);
    if (!file) {
        ofLogError() <<"ReadLabelsFile: " << file_name << " not found.";
        return false;
    }

    result.clear();
    string line;
    while (std::getline(file, line)) {
        result.push_back(line);
    }
    const int padding = 16;
    while (result.size() % padding) {
        result.emplace_back();
    }
    return true;
}


}
}
