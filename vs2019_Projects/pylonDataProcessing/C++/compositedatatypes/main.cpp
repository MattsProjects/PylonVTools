/*
    Sample using the composite data types (license required)
*/
// Include files to use the pylon API.
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#  include <pylon/PylonGUI.h>
#endif
// Extend the pylon API for using pylon data processing.
#include <pylondataprocessing/PylonDataProcessingIncludes.h>
// The sample uses the std::list.
#include <list>

// Namespaces for using pylon objects
using namespace Pylon;
using namespace Pylon::DataProcessing;

// Namespace for using cout
using namespace std;

// Number of images to be grabbed
static const uint32_t c_countOfImagesToGrab = 24;

// Declare a data class for one set of output data values.
class ResultData
{
public:
    ResultData()
        : hasError(false)
    {
    }

    // Simple struct to store the data of the composite data type
    struct RectangleF
    {
        double      centerX     = 0.;
        double      centerY     = 0.;
        uint64_t    width       = 0;
        uint64_t    height      = 0;
        double      rotation    = 0.;
    };

    CPylonImage         image; // The image is only used for displaying
    list<RectangleF>    boxes; // List of bounding boxes

    bool hasError;     // If something doesn't work as expected
                       // while processing data, this is set to true.

    String_t errorMessage; // Contains an error message if
                           // hasError has been set to true.
};

// MyOutputObserver is a helper object that shows how to handle output data
// provided via the IOutputObserver::OutputDataPush interface method.
// Also, MyOutputObserver shows how a thread-safe queue can be implemented
// for later processing while pulling the output data.
class MyOutputObserver : public IOutputObserver
{
public:
    MyOutputObserver()
        : m_waitObject(WaitObjectEx::Create())
    {
    }

    // Implements IOutputObserver::OutputDataPush.
    // This method is called when an output of the CRecipe pushes data out.
    // The call of the method can be performed by any thread of the thread pool of the recipe.
    void OutputDataPush(
        CRecipe& recipe,
        CVariantContainer valueContainer,
        const CUpdate& update,
        intptr_t userProvidedId) override
    {
        // The following variables are not used here:
        PYLON_UNUSED(recipe);
        PYLON_UNUSED(update);
        PYLON_UNUSED(userProvidedId);

        ResultData currentResultData;

        // Get the result data of the recipe via the output terminal's "Image" pin.
        // The value container is a dictionary/map-like type.
        // Look for the key in the dictionary.
        auto posImage = valueContainer.find("Image");
        // We expect there to be an image
        // because the recipe is set up to behave like this.
        PYLON_ASSERT(posImage != valueContainer.end());
        if (posImage != valueContainer.end())
        {
            // Now we can use the value of the key/value pair.
            const CVariant& value = posImage->second;
            if (!value.HasError())
            {
                currentResultData.image = value.ToImage();
            }
            else
            {
                currentResultData.hasError = true;
                currentResultData.errorMessage = value.GetErrorDescription();
            }
        }

        auto posBoxes = valueContainer.find("Boxes");
        PYLON_ASSERT(posBoxes != valueContainer.end());
        if (posBoxes != valueContainer.end())
        {
            // Now we can use the value of the key/value pair.
            const CVariant& value = posBoxes->second;
            if (!value.HasError())
            {
                // The value should be an array
                if (value.IsArray() == true)
                {
                    // Loop over all array elements
                    for (size_t i = 0; i < value.GetNumArrayValues(); ++i)
                    {
                        // Get an element of the array
                        const CVariant& variant = value.GetArrayValue(i);

                        // The variant arrayElement has the data type VariantDataType_Composite and is defined as follows:
                        // Composite {
                        //   "Center" : Composite {
                        //      "X" : Float,
                        //      "Y" : Float
                        //   },
                        //   Width    : Integer,
                        //   Height   : Integer,
                        //   Rotation : Float
                        // }
                        //
                        // Types can be obtained by calling GetDataType(). Sub value names can be obtained with GetSubValueName(size_t index) with the help of GetNumSubValues().

                        try
                        {
                            // The data from variant will be stored in the RectangleF struct
                            ResultData::RectangleF rectangle;

                            // The center values are a nested composite data type. Because of that, the sub value "X" of "Center" is retrieved like a path
                            rectangle.centerX = variant.GetSubValue("Center").GetSubValue("X").ToDouble();
                            rectangle.centerY = variant.GetSubValue("Center").GetSubValue("Y").ToDouble();

                            // Width, height and rotation are sub values of the variant, so they can be directly accessed
                            rectangle.width = variant.GetSubValue("Width").ToUInt64();
                            rectangle.height = variant.GetSubValue("Height").ToUInt64();
                            rectangle.rotation = variant.GetSubValue("Rotation").ToDouble();

                            currentResultData.boxes.push_back(rectangle);
                        }
                        catch (const GenICam::GenericException& e)
                        {
                            currentResultData.hasError = true;
                            currentResultData.errorMessage = e.GetDescription();
                        }
                    }
                }
            }
            else
            {
                currentResultData.hasError = true;
                currentResultData.errorMessage = value.GetErrorDescription();
            }
        }

        // Add data to the result queue in a thread-safe way.
        {
            AutoLock scopedLock(m_memberLock);
            m_queue.emplace_back(currentResultData);
        }

        // Signal that data is ready.
        m_waitObject.Signal();
    }

    // Get the wait object for waiting for data.
    const WaitObject& GetWaitObject()
    {
        return m_waitObject;
    }

    // Get one result data object from the queue.
    bool GetResultData(ResultData& resultDataOut)
    {
        AutoLock scopedLock(m_memberLock);
        if (m_queue.empty())
        {
            return false;
        }

        resultDataOut = std::move(m_queue.front());
        m_queue.pop_front();
        if (m_queue.empty())
        {
            m_waitObject.Reset();
        }
        return true;
    }

private:
    CLock m_memberLock;        // The member lock is required to ensure
                               // thread-safe access to the member variables.
    WaitObjectEx m_waitObject; // Signals that ResultData is available.
                               // It is set if m_queue is not empty.
    list<ResultData> m_queue;  // The queue of ResultData
};

int main(int /*argc*/, char* /*argv*/[])
{
    // The exit code of the sample application.
    int exitCode = 0;

    // Enable the pylon camera emulator to provide images from disk
    // by setting the necessary environment variable.
#if defined(PYLON_WIN_BUILD)
    _putenv("PYLON_CAMEMU=1");
#elif defined(PYLON_UNIX_BUILD)
    setenv("PYLON_CAMEMU", "1", true);
#endif

    // Before using any pylon methods, the pylon runtime must be initialized.
    PylonInitialize();

    try
    {
        // This object is used for collecting the output data.
        // If placed on the stack, it must be created before the recipe
        // so that it is destroyed after the recipe.
        MyOutputObserver resultCollector;

        // Create a recipe object representing a recipe file created using
        // the pylon Viewer Workbench.
        CRecipe recipe;

        // Load the recipe file.
        // Note: PYLON_DATAPROCESSING_COMPOSITE_DATA_TYPES_RECIPE is a string
        // created by the CMake build files.
        recipe.Load(PYLON_DATAPROCESSING_COMPOSITE_DATA_TYPES_RECIPE);

        // Now we allocate all resources we need. This includes the camera device.
        recipe.PreAllocateResources();

        // Set up correct image path to samples.
        // Note: PYLON_DATAPROCESSING_SHAPE_IMAGES_PATH is a string created by the CMake build files.
        recipe.GetParameters().Get(StringParameterName("MyCamera/@CameraDevice/ImageFilename")).SetValue(PYLON_DATAPROCESSING_SHAPE_IMAGES_PATH);

        // This is where the output goes.
        recipe.RegisterAllOutputsObserver(&resultCollector, RegistrationMode_Append);

        // Start the processing.
        recipe.Start();

        for (uint32_t i = 0; i < c_countOfImagesToGrab; ++i)
        {
            if (resultCollector.GetWaitObject().Wait(5000))
            {
                ResultData result;
                resultCollector.GetResultData(result);
                if (!result.hasError)
                {
                    cout << "########## Image " << i << " ##########" << endl << endl;
                    for (const ResultData::RectangleF& box : result.boxes)
                    {
                        cout << "RectangleF {"<< endl
                             << "  Center: " << "{" << endl
                             << "    X: " << box.centerX << "," << endl
                             << "    Y: " << box.centerY << endl
                             << "  }," << endl
                             << "  Width:    " << box.width << "," << endl
                             << "  Height:   " << box.height << "," << endl
                             << "  Rotation: " << box.rotation << endl
                             << "}" << endl;
                    }

                    cout << endl << endl << endl;

#ifdef PYLON_WIN_BUILD
                    DisplayImage(1, result.image);
#endif
                }
                else
                {
                    cout << "An error occurred during processing: " << result.errorMessage << endl;
                }
            }
            else
            {
                throw RUNTIME_EXCEPTION("Result timeout");
            }
        }

        // Stop the processing.
        recipe.Stop();
    }
    catch (const GenericException& e)
    {
        // Error handling
        cerr << "An exception occurred." << endl << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    cerr << endl << "Press enter to exit." << endl;
    while (cin.get() != '\n');

    // Releases all pylon resources.
    PylonTerminate();

    return exitCode;
}
