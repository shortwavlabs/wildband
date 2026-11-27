#include "MicPermission.h"
#import <AVFoundation/AVFoundation.h>

namespace wildband
{
MicAccess microphoneAccess()
{
    const auto status = [AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeAudio];
    if (status == AVAuthorizationStatusAuthorized) return MicAccess::Granted;
    return status == AVAuthorizationStatusNotDetermined ? MicAccess::Undetermined : MicAccess::Denied;   // denied or restricted
}

void requestMicrophone (std::function<void (bool)> done)
{
    [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio completionHandler: ^(BOOL granted)
    {
        dispatch_async (dispatch_get_main_queue(), ^{ done (granted); });
    }];
}
}
