#pragma once

namespace CrashTest
{
	// Editor command는 요청만 남긴다. 실제 사고는 다음 engine update의 main thread에서 발생한다.
	void Request();
	void Update();
}
